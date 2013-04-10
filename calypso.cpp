#include "calypso.h"
#include "log_interface.h"
#include "calypso_bootstrap_config.h"
#include "calypso_util.h"
#include <tr1/functional>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

using namespace std;
using namespace std::tr1;
using namespace log4cplus;

// app 线程函数
void* _app_thread_proc(void* args);

calypso_main_t::calypso_main_t()
{
    subs_ = NULL;
    handler_ = NULL;
    out_msg_buf_ = NULL;
    out_msg_buf_size_ = 0;
}

calypso_main_t::~calypso_main_t()
{
    if (subs_)
    {
        delete []subs_;
        subs_ = NULL;
    }
}

int calypso_main_t::initialize(const char* bootstrap_path)
{
    int ret;
    calypso_bootstrap_config_t bootstrap_config;
    ret = bootstrap_config.load(bootstrap_path);
    if (ret < 0)
    {
        C_FATAL("load bootstrap %s failed ret %d", bootstrap_path, ret);
        exit(-1);
    }

    const calypso_bootstrap_config_t::alloc_conf_list_t& alloc_conf = bootstrap_config.get_allocator_config();
    int sub_allocator_num = alloc_conf.size();
    allocator_.initialize(sub_allocator_num);
    subs_ = new fixed_size_allocator_t[sub_allocator_num];
    for (int i = 0; i < sub_allocator_num; ++i)
    {
        ret = subs_[i].initialize(alloc_conf[i].size_, alloc_conf[i].capacity_);
        if (ret < 0)
        {
            C_FATAL("init sub allocator(size=%d capacity=%d) failed ret %d", alloc_conf[i].size_, alloc_conf[i].capacity_, ret);
            exit(-1);
        }

        C_DEBUG("init sub allocator(size=%d capacity=%d) succ", alloc_conf[i].size_, alloc_conf[i].capacity_);
        allocator_.add_allocator(subs_[i]);
    }

    // load runtime config
    ret = runtime_config_.load(bootstrap_config.get_runtime_config_path());
    if (ret < 0)
    {
        C_FATAL("load runtime config(%s) failed ret %d", bootstrap_config.get_runtime_config_path(), ret);
        exit(-1);
    }

    network_.init(bootstrap_config.get_max_link_num(), bootstrap_config.get_max_fired_link_num(), &allocator_);

    ret = netconfig_.load(bootstrap_config.get_netlink_config_path());
    if (ret < 0)
    {
        C_FATAL("load netconfig %s failed ret %d", bootstrap_config.get_netlink_config_path(), ret);
        exit(-1);
    }

    // create all links
    netlink_config_t::walk_link_callback create_link_func = std::tr1::bind(&calypso_main_t::create_link, this, tr1::placeholders::_1, tr1::placeholders::_2, tr1::placeholders::_3);
    netconfig_.walk(create_link_func, NULL);

    // create default out msg buffer
    out_msg_buf_size_ = 1024;
    out_msg_buf_ = allocator_.alloc(out_msg_buf_size_);
    if (NULL == out_msg_buf_)
    {
        C_FATAL("create out msg buf(%d) failed, oom?", out_msg_buf_size_);
        exit(-1);
    }

    running_app_ = 0;
    for (int i = 0; i < CALYPSO_APP_THREAD_SWITCH_NUM; ++i)
    {
        memset(&app_ctx_[i], 0, sizeof(app_ctx_[i]));
        app_ctx_[i].th_status_ = app_thread_context_t::app_stop;
    }

    nowtime_ = time(NULL);
    network_.refresh_nowtime(nowtime_);
    return 0;
}

void calypso_main_t::main()
{
    int evt_num;
    int ret;
    calypso_network_t::onevent_callback onevent_func = std::tr1::bind(&calypso_main_t::on_net_event, this, tr1::placeholders::_1, tr1::placeholders::_2, tr1::placeholders::_3, tr1::placeholders::_4);
    while (!need_stop())
    {
        nowtime_ = time(NULL);
        network_.refresh_nowtime(nowtime_);
        // retry error connection
        network_.recover(runtime_config_.get_max_recover_link_num());

        // check idle
        network_.check_idle_netlink(runtime_config_.get_max_check_link_num(), runtime_config_.get_max_tcp_idle(), runtime_config_.get_connect_timeout());

        // wait network
        evt_num = network_.wait(onevent_func, NULL);

        for (int i = 0; i < CALYPSO_APP_THREAD_SWITCH_NUM; ++i)
        {
            process_appthread_msg(app_ctx_[i]);
        }
        
        // need restart?
        if (need_restart_app() || app_ctx_[running_app_].fatal_)
        {
            clear_restart_app_sig();
            ret = restart_app();
            if (ret < 0)
            {
                C_FATAL("restart appthread on sig ret %d", ret);
                exit(-1);
            }
        }

        check_deprecated_threads(app_ctx_[(running_app_+1)%CALYPSO_APP_THREAD_SWITCH_NUM]);
        if (0 == evt_num)
        {
            usleep(50000);
        }
    }

    if (need_stop())
    {
        C_INFO("recv stop signal!%s", "");
    }
}

int calypso_main_t::create_link( int idx, const netlink_config_t::config_item_t& config, void* up )
{
    return network_.create_link(config);
}

int calypso_main_t::on_net_event( int link_idx, netlink_t& link, unsigned int evt, void* up)
{
    if (evt & error_event) return 0;

    msgpack_context_t msgpack_ctx;
    memset(&msgpack_ctx, 0, sizeof(msgpack_ctx));
    msgpack_ctx.link_ctx_ = link_idx;
    msgpack_ctx.local_ = link.get_local_addr();
    msgpack_ctx.remote_ = link.get_remote_addr();
    msgpack_ctx.link_type_ = link.get_link_type();
    int fd = link.getfd();
    char addr_str[64];
    int ret;
    if (evt & newlink_event)
    {
        msgpack_ctx.flag_ |= mpf_new_connection;
        ret = dispatch_msg_to_app(msgpack_ctx, NULL, 0);
        if (ret < 0)
        {
            char remote_addr_str[64];
            C_ERROR("sendmsg_to_appthread(new connection from %s) failed(%d)", 
                link.get_remote_addr_str(remote_addr_str, sizeof(remote_addr_str)), ret);
            return -1;
        }

        return 0;
    }

    if (evt & data_arrival_event)
    {
        ret = link.recv();
        if (ret < 0)
        {
            C_ERROR("link(fd:%d) recv failed", fd);
            link.close();
            return -1;
        }

        if (link.is_closed())
        {
            C_WARN("link(fd:%d) is closed by peer", fd);
            msgpack_ctx.flag_ |= mpf_closed_by_peer;
        }

        int data_len = 0;
        char* data = link.get_recv_buffer(data_len);
        C_DEBUG("recv %d bytes from %s", data_len, link.get_remote_addr_str(addr_str, sizeof(addr_str)));
        // has full msgpack? pop recv buf
        int pack_len = handler_->get_msgpack_size(msgpack_ctx, data, data_len);
        if (pack_len < 0)
        {
            C_ERROR("link(fd:%d) has bad msgpack, close it", fd);
            link.close();
            return -1;
        }
        else if (pack_len > 0)
        {
            ret = dispatch_msg_to_app(msgpack_ctx, data, pack_len);
            if (ret < 0)
            {
                C_ERROR("sendmsg_to_appthread(%p,%d) failed, ret %d", data, pack_len, ret);
            }

            // NOTE: 这里可能产生消息丢失的问题
            ret = link.pop_recv_buffer(pack_len);
            if (ret != pack_len)
            {
                C_FATAL("pop_recv_buffer(%d) but return %d", pack_len, ret);
            }
        }
    }

    return 0;
}

void calypso_main_t::run()
{
    init_rand();

    int ret = create_appthread(app_ctx_[running_app_]);
    if (ret < 0)
    {
        C_FATAL("create appthread failed %d", ret);
        exit(-1);
    }

    main();
}

int calypso_main_t::send_by_group( int group, const char* data, size_t len )
{
    int linkid = netconfig_.get_rand_linkid_by_group(group);
    netlink_t* link = network_.find_link(linkid);
    if (NULL == link)
    {
        C_ERROR("find link by %d failed, group %d", linkid, group);
        return -1;
    }

    return link->send(data, len);
}

void calypso_main_t::broadcast_by_group( int group, const char* data, size_t len )
{
    vector<int> linkids = netconfig_.get_linkid_by_group(group);
    netlink_t* link;
    int ret;
    for (int i = 0; i < (int)linkids.size(); ++i)
    {
        link = network_.find_link(linkids.at(i));
        if (NULL == link)
        {
            C_ERROR("findlink %d ret NULL", linkids.at(i));
            continue;
        }

        ret = link->send(data, len);
        if (ret < 0)
        {
            C_ERROR("send(%p, %d) failed(%d) when broadcast group %d", data, (int)len, ret, group);
        }
    }
}

int calypso_main_t::send_by_context( msgpack_context_t ctx, const char* data, size_t len )
{
    netlink_t* link = network_.find_link(ctx.link_ctx_);
    if (NULL == link)
    {
        C_ERROR("find no link by idx %d", ctx.link_ctx_);
        return -1;
    }

    // 校验一下
    sockaddr_in netaddr;
    char link_addr_str[64];
    switch (ctx.link_type_)
    {
    case netlink_t::client_link:
        netaddr = link->get_remote_addr();
        if (0 != memcmp(&netaddr, &ctx.remote_, sizeof(ctx.remote_)))
        {
            C_ERROR("unmatch remote addr! %s", link->get_remote_addr_str(link_addr_str, sizeof(link_addr_str)));
            return -1;
        }

        break;
    case netlink_t::accept_link:
        netaddr = link->get_local_addr();
        if (0 != memcmp(&netaddr, &ctx.local_, sizeof(ctx.local_)))
        {
            C_ERROR("unmatch local addr! %s", link->get_local_addr_str(link_addr_str, sizeof(link_addr_str)));
            return -1;
        }

        break;
    default:
        C_ERROR("invalid link type %d", ctx.link_type_);
        return -1;
    }

    int ret = link->send(data, len);
    C_TRACE("send data(%p, %d) to %s ret %d", data, (int)len, link->get_remote_addr_str(link_addr_str, sizeof(link_addr_str)), ret);
    if (ret < 0)
    {
        C_ERROR("send data(%p, %d) to %s failed(%d)", data, (int)len, link->get_remote_addr_str(link_addr_str, sizeof(link_addr_str)), ret);
    }

    return ret;
}

int calypso_main_t::dispatch_msg_to_app( const msgpack_context_t& msgctx, const char* data, size_t len )
{
    if (NULL == data) len = 0;
    app_thread_context_t& cur_ctx = app_ctx_[running_app_];
    int reserve_len = sizeof(msgctx)+len;
    int rctx = cur_ctx.in_->produce_reserve(reserve_len);
    if (rctx < 0)
    {
        C_ERROR("produce_reserve(%d) failed ret %d, oom? free=%d", reserve_len, rctx, cur_ctx.in_->get_free_len());
        return -1;
    }

    bool rq_err = true;
    do 
    {
        int ret = cur_ctx.in_->produce_append((const char*)&msgctx, sizeof(msgctx));
        if (ret < 0)
        {
            C_FATAL("produce msg ctx failed ret %d", ret);
            break;
        }

        if (data && len > 0)
        {
            ret = cur_ctx.in_->produce_append(data, len);
            if (ret < 0)
            {
                C_FATAL("produce msg content failed ret %d", ret);
                break;
            }
        }

        ret = cur_ctx.in_->produce_append(NULL, 0);
        if (ret < 0)
        {
            C_FATAL("finishing produce msg failed ret %d", ret);
            break;
        }

        rq_err = false;
    } while (false);

    if (rq_err)
    {
        C_FATAL("find error in ring queue, try restart app thread! data(%p,%d)", data, (int)len);
        cur_ctx.fatal_ = 1;
        return -1;
    }

    return 0;
}

int calypso_main_t::create_appthread(app_thread_context_t& thread_ctx)
{
    memset(&thread_ctx, 0, sizeof(thread_ctx));
    int ret = pthread_attr_init(&thread_ctx.th_attr_);
    if (ret < 0)
    {
        C_FATAL("pthread_attr_init ret %d, errno %d", ret, errno);
        return -1;
    }

    ret = pthread_attr_setscope(&thread_ctx.th_attr_, PTHREAD_SCOPE_SYSTEM);
    if (ret < 0)
    {
        C_FATAL("pthread_attr_setscope ret %d, errno %d", ret, errno);
        return -1;
    }

    ret = pthread_attr_setdetachstate(&thread_ctx.th_attr_, PTHREAD_CREATE_JOINABLE);
    if (ret < 0)
    {
        C_FATAL("pthread_attr_setdetachstate ret %d, errno %d", ret, errno);
        return -1;
    }

    ret = pthread_cond_init(&thread_ctx.th_cond_, NULL);
    if (ret < 0)
    {
        C_FATAL("pthread_cond_init ret %d, errno %d", ret, errno);
        return -1;
    }

    ret = pthread_mutex_init(&thread_ctx.th_mutex_, NULL);
    if (ret < 0)
    {
        C_FATAL("pthread_mutex_init ret %d, errno %d", ret, errno);
        return -1;
    }

    thread_ctx.handler_ = handler_;
    thread_ctx.allocator_ = &allocator_;
    thread_ctx.in_ = new ring_queue_t();
    if (NULL == thread_ctx.in_)
    {
        C_FATAL("create appthread in queue failed %p", thread_ctx.in_);
        return -1;
    }

    ret = thread_ctx.in_->initialize(runtime_config_.get_app_queue_len());
    if (ret < 0)
    {
        C_FATAL("init inqueue(%d) failed ret %d", runtime_config_.get_app_queue_len(), ret);
        return -1;
    }

    thread_ctx.out_ = new ring_queue_t();
    if (NULL == thread_ctx.out_)
    {
        C_FATAL("create appthread out queue failed %p", thread_ctx.out_);
        return -1;
    }

    ret = thread_ctx.out_->initialize(runtime_config_.get_app_queue_len());
    if (ret < 0)
    {
        C_FATAL("init outqueue(%d) failed ret %d", runtime_config_.get_app_queue_len(), ret);
        return -1;
    }

    thread_ctx.th_status_ = app_thread_context_t::app_running;
    // ONE LAST STEP
    pthread_create(&thread_ctx.th_, &thread_ctx.th_attr_, _app_thread_proc, (void *)&thread_ctx );
    if (ret < 0)
    {
        C_FATAL("pthread_create ret %d, errno %d", ret, errno);
        return -1;
    }

    C_INFO("app thread created%s", "");
    return 0;
}

void calypso_main_t::cleanup()
{
    for (int i = 0; i < CALYPSO_APP_THREAD_SWITCH_NUM; ++i)
    {
        stop_appthread(app_ctx_[i]);
    }
}

void calypso_main_t::stop_appthread(app_thread_context_t& thread_ctx)
{
    pthread_mutex_lock(&thread_ctx.th_mutex_);
    thread_ctx.th_status_ = app_thread_context_t::app_stop;
    pthread_cond_signal(&thread_ctx.th_cond_);
    pthread_mutex_unlock(&thread_ctx.th_mutex_);
    pthread_join(thread_ctx.th_, NULL);
    if (thread_ctx.in_)
    {
        delete thread_ctx.in_;
        thread_ctx.in_ = NULL;
    }

    if (thread_ctx.out_)
    {
        delete thread_ctx.out_;
        thread_ctx.out_ = NULL;
    }
}

int calypso_main_t::process_appthread_msg(app_thread_context_t& thread_ctx)
{
    int clen;
    msgpack_context_t msgctx;
    int proc_num = 0;
    int data_len;
    
    while (thread_ctx.th_status_ != app_thread_context_t::app_stop)
    {
        clen = thread_ctx.out_->get_consume_len();
        if (clen <= 0)
        {
            if (clen < 0)
            {
                C_FATAL("get_consume_len ret %d, maybe broken", clen);
                thread_ctx.fatal_ = 1;
                return -1;
            }

            break;
        }

        ++proc_num;
        if (clen > out_msg_buf_size_)
        {
            C_WARN("outmsg buf too small(%d), ready to extend(%d)", out_msg_buf_size_, clen);
            char* newbuf = allocator_.realloc(clen, out_msg_buf_);
            if (NULL == newbuf)
            {
                C_ERROR("realloc out msg buf(%d) failed", clen);
                thread_ctx.out_->skip_consume();
                continue;
            }

            out_msg_buf_ = newbuf;
            out_msg_buf_size_ = clen;
        }

        thread_ctx.out_->consume(out_msg_buf_, out_msg_buf_size_);
        // 检查msgctx
        data_len = clen - sizeof(msgctx);
        memcpy(&msgctx, out_msg_buf_, sizeof(msgctx));
        if (data_len > 0)
        {
            send_by_context(msgctx, &out_msg_buf_[sizeof(msgctx)], data_len);
        }
    }

    if (proc_num > 0)
    {
        thread_ctx.last_busy_time_ = nowtime_;
    }

    return proc_num;
}

int calypso_main_t::restart_app()
{
    int next_idx = (running_app_+1)%CALYPSO_APP_THREAD_SWITCH_NUM;
    int ret = restart_appthread(app_ctx_[running_app_], app_ctx_[next_idx]);
    if (ret < 0)
    {
        return ret;
    }

    running_app_ = next_idx;
    return ret;
}

int calypso_main_t::restart_appthread(app_thread_context_t& old_ctx, app_thread_context_t& new_ctx)
{
    // NOTE:从此不再向旧线程发送消息，但是依然会处理它的输出
    if (new_ctx.th_status_ != app_thread_context_t::app_stop)
    {
        C_WARN("deprecated thread ctx still running(%d)? this may cauz memleak", new_ctx.th_status_);
        // dont exit
    }

    old_ctx.th_status_ = app_thread_context_t::app_deprecated;
    old_ctx.deprecated_time_ = nowtime_;
    int ret = create_appthread(new_ctx);
    if (ret < 0)
    {
        C_ERROR("create_appthread failed ret %d", ret);
        return -1;
    }

    return 0;
}

void calypso_main_t::check_deprecated_threads(app_thread_context_t& thread_ctx)
{
    if (thread_ctx.th_status_ != app_thread_context_t::app_deprecated)
    {
        return;
    }

    // stop thread?
    if (nowtime_ - thread_ctx.deprecated_time_ >= runtime_config_.get_deprecated_thread_life()
        || nowtime_ - thread_ctx.last_busy_time_ >= runtime_config_.get_deprecated_thread_idle_life())
    {
        C_INFO("now stopping deprecated appthread!%s", "");
        stop_appthread(thread_ctx);
        C_INFO("deprecated appthread stopped!%s", "");
    }
}

void* _app_thread_proc(void* args)
{
    app_thread_context_t* ctx = (app_thread_context_t*)args;
    int handle_msg_num;
    int ret;
    msgpack_context_t msgctx;
    int msg_buf_size = 1024;    // 初始大小
    char *msg_buf = new char[msg_buf_size];
    if (NULL == msg_buf)
    {
        C_FATAL("create msg buf for app thread failed %p", msg_buf);
        return NULL;
    }

    int clen;
    bool fatal = false;
    while (ctx->th_status_ != app_thread_context_t::app_stop && !fatal)
    {
        // loop callback
        ctx->handler_->handle_tick();

        handle_msg_num = 0;
        while (!fatal)
        {
            // 处理inqueue消息
            // 消息格式：[msgctx][data..]
            clen = ctx->in_->get_consume_len();
            if (msg_buf_size < clen)
            {
                char* newbuf = new char[clen];
                if (NULL == newbuf)
                {
                    C_FATAL("no more memory for this msgpack(len:%d), skip it", clen);
                    ctx->in_->skip_consume();
                    continue;
                }

                delete []msg_buf;
                msg_buf = newbuf;
                msg_buf_size = clen;
                C_DEBUG("extend out msg buf to %d", msg_buf_size);
            }

            ret = ctx->in_->consume(msg_buf, msg_buf_size);
            if (ret < 0)
            {
                C_FATAL("consume msg queue failed ret %d, broken pipe?", ret);
                fatal = true;
                break;
            }
            else if (ret > 0)
            {
                C_TRACE("recv msgpack from mainthread, len:%d", clen);
                memcpy(&msgctx, msg_buf, sizeof(msgctx));
                ret = ctx->handler_->handle_msgpack(msgctx, &msg_buf[sizeof(msgctx)], clen - sizeof(msgctx));
                // TODO: handle stat
                ++handle_msg_num;
            }
            else
            {
                // no more msg
                break;
            }
        }

        if (0 == handle_msg_num)
        {
            usleep(50000);
        }
    }

    if (msg_buf)
    {
        delete []msg_buf;
    }

    return NULL;
}