#include "calypso.h"
#include "log_interface.h"
#include "calypso_bootstrap_config.h"
#include "calypso_signal.h"
#include "utility.h"
#include "app_interface.h"
#include <tr1/functional>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

using namespace std;
using namespace std::tr1;
using namespace log4cplus;

// app thread proc function
void* _app_thread_main(void* args);

calypso_main_t::calypso_main_t()
{
    subs_ = NULL;
    out_msg_buf_ = NULL;
    out_msg_buf_size_ = 0;
    app_dl_handler_ = NULL;
    memset(&handler_, 0, sizeof(handler_));
    app_ctx_ = NULL;
}

calypso_main_t::~calypso_main_t()
{
    // 1.remove network
    network_.fina();
    // 2.remove dynamic allocators
    allocator_.remove_allocator();
    // 3.remove sub allocators
    if (subs_)
    {
        delete []subs_;
        subs_ = NULL;
    }

    if (app_ctx_)
    {
        cleanup();
        delete []app_ctx_;
        app_ctx_ = NULL;
    }
}

int calypso_main_t::initialize(const char* bootstrap_path)
{
    // base data
    nowtime_ = time(NULL);
    last_reload_config_ = 0;
    last_stat_ = nowtime_;

    int ret;
    // load bootstrap
    ret = bootstrap_config_.load(bootstrap_path);
    if (ret < 0)
    {
        C_FATAL("load bootstrap %s failed ret %d", bootstrap_path, ret);
        exit(-1);
    }

    // init allocator
    const calypso_bootstrap_config_t::alloc_conf_list_t& alloc_conf = bootstrap_config_.get_allocator_config();
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
    ret = runtime_config_.load(bootstrap_config_.get_runtime_config_path());
    if (ret < 0)
    {
        C_FATAL("load runtime config(%s) failed ret %d", bootstrap_config_.get_runtime_config_path(), ret);
        exit(-1);
    }
    // init network data
    ret = network_.init(bootstrap_config_.get_max_link_num(), bootstrap_config_.get_max_fired_link_num(), &allocator_);
    if (ret < 0)
    {
        C_FATAL("init network(maxlink=%d, maxfired=%d) failed, ret %d", 
            bootstrap_config_.get_max_link_num(), bootstrap_config_.get_max_fired_link_num(), ret);
        exit(-1);
    }
    // load network config
    ret = netconfig_.load(bootstrap_config_.get_netlink_config_path());
    if (ret < 0)
    {
        C_FATAL("load netconfig %s failed ret %d", bootstrap_config_.get_netlink_config_path(), ret);
        exit(-1);
    }
    // create all links
    netlink_config_t::walk_link_callback create_link_func = std::tr1::bind(&calypso_main_t::create_link, this, 
        tr1::placeholders::_1, tr1::placeholders::_2, tr1::placeholders::_3);
    netconfig_.walk(create_link_func, NULL);

    // create default out msg buffer
    out_msg_buf_size_ = 1024;
    out_msg_buf_ = allocator_.alloc(out_msg_buf_size_);
    if (NULL == out_msg_buf_)
    {
        C_FATAL("create out msg buf(%d) failed, oom?", out_msg_buf_size_);
        exit(-1);
    }

    // init threads data
    if (bootstrap_config_.get_thread_num() <= 0)
    {
        C_FATAL("invalid threadnum %d", bootstrap_config_.get_thread_num());
        exit(-1);
    }

    app_ctx_ = new app_thread_context_t[bootstrap_config_.get_thread_num()];
    if (NULL == app_ctx_)
    {
        C_FATAL("create %d threads context failed", bootstrap_config_.get_thread_num());
        exit(-1);
    }

    for (int i = 0; i < bootstrap_config_.get_thread_num(); ++i)
    {
        memset(&app_ctx_[i], 0, sizeof(app_ctx_[i]));
        app_ctx_[i].th_status_ = app_thread_context_t::app_stop;
        app_ctx_[i].th_idx_ = i;
    }

    // stat
    ret = stat_.init(bootstrap_config_.get_thread_num());
    if (ret < 0)
    {
        C_FATAL("init stat(%d) failed %d", bootstrap_config_.get_thread_num(), ret);
        exit(-1);
    }

    return 0;
}

void calypso_main_t::main()
{
    int evt_num;
    int proc_app_msg_num;
    calypso_network_t::onevent_callback onevent_func = std::tr1::bind(&calypso_main_t::on_net_event, this, tr1::placeholders::_1, tr1::placeholders::_2, tr1::placeholders::_3, tr1::placeholders::_4);
    while (!need_stop())
    {
        evt_num = 0;
        proc_app_msg_num = 0;
        nowtime_ = time(NULL);
        network_.refresh_nowtime(nowtime_);
        if (need_reload(last_reload_config_))
        {
            reload_config();
            app_global_reload();
            last_reload_config_ = nowtime_;
        }

        // retry error connection
        network_.recover(runtime_config_.get_max_recover_link_num(), runtime_config_.get_min_recover_link_interval());

        // check idle links
        network_.check_idle_netlink(runtime_config_.get_max_check_link_num(), runtime_config_.get_max_tcp_idle(), runtime_config_.get_connect_timeout());

        // wait network
        evt_num = network_.wait(onevent_func, NULL);

        for (int i = 0; i < bootstrap_config_.get_thread_num(); ++i)
        {
            proc_app_msg_num = process_appthread_msg(app_ctx_[i]);
        }

        if (nowtime_ - last_stat_ >= runtime_config_.get_stat_interval())
        {
            last_stat_ = nowtime_;
            for (int i = 0; i < bootstrap_config_.get_thread_num(); ++i)
            {
                stat_.set_thread_inq_free(i, app_ctx_[i].in_->get_free_len());
                stat_.set_thread_outq_free(i, app_ctx_[i].out_->get_free_len());
            }

            stat_.write_and_clear();
        }

        if (0 == evt_num && proc_app_msg_num <= 0)
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

int calypso_main_t::close_link( int idx, const netlink_config_t::config_item_t& config, void* up )
{
    return network_.close_link(idx);
}

int calypso_main_t::update_link( int idx, const netlink_config_t::config_item_t& config, void* up )
{
    return network_.update_link(idx, config);
}

int calypso_main_t::on_net_event( int link_idx, netlink_t& link, unsigned int evt, void* up)
{
    // NOTE: should we notify appthread the error event?
    if (evt & error_event) return 0;

    // TODO: push msg to thread[0] for now
    app_thread_context_t& cur_ctx = app_ctx_[0];
    msgpack_context_t msgpack_ctx;
    memset(&msgpack_ctx, 0, sizeof(msgpack_ctx));
    msgpack_ctx.link_ctx_ = link_idx;
    msgpack_ctx.link_fd_ = link.getfd();
    msgpack_ctx.local_ = link.get_local_addr();
    msgpack_ctx.remote_ = link.get_remote_addr();
    msgpack_ctx.link_type_ = link.get_link_type();
    int fd = link.getfd();
    int ret;
    if (evt & newlink_event)
    {
        if (link.is_bit_allowed(mpf_new_connection))
        {
            msgpack_ctx.flag_ |= mpf_new_connection;
        }

        if (0 == msgpack_ctx.flag_) return 0;
        stat_.incr_accept_conn(1);
        ret = dispatch_msg_to_app(cur_ctx, msgpack_ctx, NULL, 0, NULL);
        if (ret < 0)
        {
            char remote_addr_str[64];
            C_ERROR("dispatch_msg_to_app(new connection from %s) failed(%d)", 
                link.get_remote_addr_str(remote_addr_str, sizeof(remote_addr_str)), ret);
            return -1;
        }

        return 0;
    }

    if (evt & data_arrival_event)
    {
        bool closed_by_peer = false;
        ret = link.recv();
        if (ret < 0)
        {
            C_ERROR("link(fd:%d) recv failed, now close it", fd);
            link.close();
            stat_.incr_error_conn(1);
            return -1;
        }

        stat_.incr_recv_bytes(ret);
        if (link.is_closed())
        {
            C_WARN("link(fd:%d) is closed by peer", fd);
            closed_by_peer = true;
            stat_.incr_closed_conn(1);
        }

        int data_len;
        char* data;
        int pack_len;
        char extrabuf[1024];    // 1KB enough for now
        size_t extrabuf_len;
        while (true)
        {
            msgpack_ctx.extrabuf_len_ = 0;  // clear extrbuf len
            data_len = 0;
            data = link.get_recv_buffer(data_len);
            // has full msgpack?
            extrabuf_len = sizeof(extrabuf);
            pack_len = handler_.get_msgpack_size_(&msgpack_ctx, data, data_len, extrabuf, &extrabuf_len);
            if (pack_len < 0)
            {
                C_ERROR("link(fd:%d) has bad msgpack, close it", fd);
                link.close();
                stat_.incr_error_conn(1);
                return -1;
            }
            else if (pack_len > 0 && pack_len <= data_len)
            {
                stat_.incr_recv_packs(1);
                stat_.incr_thread_recv_packs(cur_ctx.th_idx_, 1);
                stat_.incr_thread_recv_bytes(cur_ctx.th_idx_, pack_len);
                msgpack_ctx.extrabuf_len_ = extrabuf_len;
                ret = dispatch_msg_to_app(cur_ctx, msgpack_ctx, data, pack_len, extrabuf);
                if (ret < 0)
                {
                    C_ERROR("sendmsg_to_appthread(%p,%d) failed, ret %d", data, pack_len, ret);
                }

                // NOTE: dispatch failure and pop recved msg cause msg lost
                ret = link.pop_recv_buffer(pack_len);
                if (ret != pack_len)
                {
                    C_FATAL("pop_recv_buffer(%d) but return %d", pack_len, ret);
                }
            }
            else
            {
                // no full msgpack
                break;
            }
        }

        // link closed, notify appthread
        if (closed_by_peer)
        {
            msgpack_ctx.extrabuf_len_ = 0;
            if (link.is_bit_allowed(mpf_closed_by_peer))
            {
                msgpack_ctx.flag_ |= mpf_closed_by_peer;
            }

            if (0 == msgpack_ctx.flag_) return 0;
            ret = dispatch_msg_to_app(cur_ctx, msgpack_ctx, NULL, 0, NULL);
            if (ret < 0)
            {
                char remote_addr_str[64];
                C_ERROR("dispatch_msg_to_app(connection closed by %s) failed(%d)", 
                    get_addr_str(msgpack_ctx.remote_, remote_addr_str, sizeof(remote_addr_str)), ret);
                return -1;
            }
        }
    }

    return 0;
}

void calypso_main_t::run()
{
    init_rand();
    int ret;
    ret = app_global_init();
    if (ret < 0)
    {
        C_FATAL("app_global_init failed %d", ret);
        exit(-1);
    }
    
    for (int i = 0; i < bootstrap_config_.get_thread_num(); ++i)
    {
        ret = create_appthread(app_ctx_[i]);
        if (ret < 0)
        {
            C_FATAL("create appthread[%d] failed %d", i, ret);
            exit(-1);
        }
    }

    main();
    app_global_fina();
}

int calypso_main_t::send_by_group( int group, const char* data, size_t len )
{
    netlink_t* link = pick_rand_link(group);
    if (NULL == link)
    {
        C_ERROR("find no link by group %d, plz check netlinkconfig or net status", group);
        return -1;
    }

    stat_.incr_send_bytes(len);
    stat_.incr_send_packs(1);
    char link_addr_str[64];
    int ret = link->send(data, len);
    if (ret < 0)
    {
        C_ERROR("send data(%p, %d) by group to %s failed(%d)", data, (int)len, link->get_remote_addr_str(link_addr_str, sizeof(link_addr_str)), ret);
    }
    else
    {
        C_TRACE("send data(%p, %d) by group to %s succ", data, (int)len, link->get_remote_addr_str(link_addr_str, sizeof(link_addr_str)));
    }

    return ret;
}

void calypso_main_t::broadcast_by_group( int group, const char* data, size_t len )
{
    vector<int> linkids = netconfig_.get_linkid_by_group(group);
    netlink_t* link;
    int ret, sendnum = 0;
    char link_addr_str[64];
    for (int i = 0; i < (int)linkids.size(); ++i)
    {
        link = network_.find_link(linkids.at(i));
        if (NULL == link || !link->is_established())
        {
            C_ERROR("link %d not connected, plz check netlink config or net status", linkids.at(i));
            continue;
        }

        ++sendnum;
        ret = link->send(data, len);
        if (ret < 0)
        {
            C_ERROR("send(%p, %d) to %s failed(%d) when broadcast group %d", data, (int)len, 
                link->get_remote_addr_str(link_addr_str, sizeof(link_addr_str)), 
                ret, group);
        }
        else
        {
            C_TRACE("send(%p, %d) to %s in group %d succ", data, (int)len, 
                link->get_remote_addr_str(link_addr_str, sizeof(link_addr_str)), 
                group);
        }
    }

    if (sendnum > 0)
    {
        stat_.incr_send_bytes(len * sendnum);
        stat_.incr_send_packs(sendnum);
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

    if (link->is_closed())
    {
        C_ERROR("link[%d] already closed, cant send", ctx.link_ctx_);
        return -1;
    }

    // check if same link?
    if (ctx.link_fd_ != link->getfd())
    {
        char laddr_str[64];
        char raddr_str[64];
        C_ERROR("unmatch fd! ltype=%d ctx=%d link=%d ctx_local=%s ctx_remote=%s", 
            ctx.link_type_, ctx.link_fd_, link->getfd(), 
            get_addr_str(ctx.local_, laddr_str, sizeof(laddr_str)), 
            get_addr_str(ctx.remote_, raddr_str, sizeof(raddr_str)));
        return -1;
    }

    stat_.incr_send_bytes(len);
    stat_.incr_send_packs(1);
    char link_addr_str[64];
    int ret = link->send(data, len);
    if (ret < 0)
    {
        C_ERROR("send data(%p, %d) to %s failed(%d)", data, (int)len, link->get_remote_addr_str(link_addr_str, sizeof(link_addr_str)), ret);
    }
    else
    {
        C_TRACE("send data(%p, %d) to %s succ", data, (int)len, link->get_remote_addr_str(link_addr_str, sizeof(link_addr_str)));
    }

    return ret;
}

int calypso_main_t::dispatch_msg_to_app( app_thread_context_t& ctx, 
                                        const msgpack_context_t& msgctx, const char* data, size_t len, 
                                        const char* extrabuf )
{
    C_TRACE("prepare to dispatch msg(%p, %d) to app", data, (int)len);
    if (NULL == data) len = 0;
    int reserve_len = sizeof(msgctx)+len+msgctx.extrabuf_len_;
    int rctx = ctx.in_->produce_reserve(reserve_len);
    if (rctx < 0)
    {
        C_ERROR("produce_reserve(%d) failed ret %d, oom? free=%d", reserve_len, rctx, ctx.in_->get_free_len());
        return -1;
    }

    bool rq_err = true;
    do 
    {
        // msg context
        int ret = ctx.in_->produce_append((const char*)&msgctx, sizeof(msgctx));
        if (ret < 0)
        {
            C_FATAL("produce msg ctx failed ret %d", ret);
            break;
        }
        // extra buf
        if (extrabuf && msgctx.extrabuf_len_ > 0)
        {
            ret = ctx.in_->produce_append(extrabuf, msgctx.extrabuf_len_);
            if (ret < 0)
            {
                C_FATAL("produce extra(%p, %d) content failed ret %d", extrabuf, msgctx.extrabuf_len_, ret);
                break;
            }
        }
        // msgpack
        if (data && len > 0)
        {
            ret = ctx.in_->produce_append(data, len);
            if (ret < 0)
            {
                C_FATAL("produce msg(%p, %d) content failed ret %d", data, (int)len, ret);
                break;
            }
        }

        ret = ctx.in_->produce_append(NULL, 0);
        if (ret < 0)
        {
            C_FATAL("finishing produce msg failed ret %d", ret);
            break;
        }

        rq_err = false;
    } while (false);

    if (rq_err)
    {
        C_FATAL("find error in ring queue, ABORT NOW! data(%p,%d)", data, (int)len);
        ctx.fatal_ = 1;
        abort();
    }

    return 0;
}

int calypso_main_t::create_appthread(app_thread_context_t& thread_ctx)
{
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

    // in queue
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

    // out queue
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

    // handler
    thread_ctx.handler_ = &handler_;
    app_init_option opt;
    memset(&opt, 0, sizeof(opt));
    opt.msg_queue_ = thread_ctx.out_;
    opt.th_idx_ = thread_ctx.th_idx_;
    thread_ctx.app_inst_ = handler_.init_(opt);
    thread_ctx.th_status_ = app_thread_context_t::app_running;
    thread_ctx.fatal_ = 0;
    thread_ctx.last_busy_time_ = 0;

    // ONE LAST STEP
    pthread_create(&thread_ctx.th_, &thread_ctx.th_attr_, _app_thread_main, (void *)&thread_ctx );
    if (ret < 0)
    {
        C_FATAL("pthread_create ret %d, errno %d", ret, errno);
        return -1;
    }

    C_INFO("app thread[%d] created", thread_ctx.th_idx_);
    return 0;
}

void calypso_main_t::cleanup()
{
    for (int i = 0; i < bootstrap_config_.get_thread_num(); ++i)
    {
        stop_appthread(app_ctx_[i]);
    }
}

void calypso_main_t::stop_appthread(app_thread_context_t& thread_ctx)
{
    int err;
    pthread_mutex_lock(&thread_ctx.th_mutex_);
    thread_ctx.th_status_ = app_thread_context_t::app_stop;
    pthread_cond_signal(&thread_ctx.th_cond_);
    pthread_mutex_unlock(&thread_ctx.th_mutex_);
    err = pthread_join(thread_ctx.th_, NULL);
    if (err)
    {
        C_FATAL("pthread_join thread[%d] error %d, errno %d", thread_ctx.th_idx_, err, errno);
    }

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

    thread_ctx.handler_->fina_(thread_ctx.app_inst_);
    thread_ctx.app_inst_ = NULL;
    C_INFO("thread[%d] stopped", thread_ctx.th_idx_);
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
                C_ERROR("realloc out msg buf(%d) failed, have to SKIP this msgpack!", clen);
                thread_ctx.out_->skip_consume();
                continue;
            }

            out_msg_buf_ = newbuf;
            out_msg_buf_size_ = clen;
        }

        thread_ctx.out_->consume(out_msg_buf_, out_msg_buf_size_);
        // copy msgctx from bytearray
        data_len = clen - sizeof(msgctx);
        memcpy(&msgctx, out_msg_buf_, sizeof(msgctx));
        if (data_len > 0)
        {
            stat_.incr_thread_send_bytes(thread_ctx.th_idx_, data_len);
            stat_.incr_thread_send_packs(thread_ctx.th_idx_, 1);

            if (tt_broadcast_group == msgctx.link_fd_)
            {
                broadcast_by_group(msgctx.link_ctx_, &out_msg_buf_[sizeof(msgctx)], data_len);
            }
            else if (tt_send_group == msgctx.link_fd_)
            {
                send_by_group(msgctx.link_ctx_, &out_msg_buf_[sizeof(msgctx)], data_len);
            }
            else if (msgctx.link_fd_ >= 0)
            {
                send_by_context(msgctx, &out_msg_buf_[sizeof(msgctx)], data_len);
            }
            else
            {
                C_ERROR("invalid link fd %d", msgctx.link_fd_);
            }
        }

        if (msgctx.flag_ & mpf_close_link)
        {
            // appthread requests to shutdown this link
            C_DEBUG("app needs to shutdown this link %d", msgctx.link_ctx_);
            network_.shutdown_link(msgctx.link_ctx_);
            stat_.incr_closed_conn(1);
        }
    }

    if (proc_num > 0)
    {
        thread_ctx.last_busy_time_ = nowtime_;
    }

    return proc_num;
}

void calypso_main_t::reload_config()
{
    // load runtime config
    int ret;
    ret = runtime_config_.load(bootstrap_config_.get_runtime_config_path());
    if (ret < 0)
    {
        C_FATAL("load runtime config(%s) failed ret %d", bootstrap_config_.get_runtime_config_path(), ret);
        return;
    }

    netlink_config_t oldcfg = netconfig_;
    ret = netconfig_.load(bootstrap_config_.get_netlink_config_path());
    if (ret < 0)
    {
        C_FATAL("load netconfig %s failed ret %d", bootstrap_config_.get_netlink_config_path(), ret);
        return;
    }

    // create all links
    netlink_config_t::walk_link_callback create_link_func = std::tr1::bind(&calypso_main_t::create_link, this, tr1::placeholders::_1, tr1::placeholders::_2, tr1::placeholders::_3);
    netlink_config_t::walk_link_callback close_link_func = std::tr1::bind(&calypso_main_t::close_link, this, tr1::placeholders::_1, tr1::placeholders::_2, tr1::placeholders::_3);
    netlink_config_t::walk_link_callback update_link_func = std::tr1::bind(&calypso_main_t::update_link, this, tr1::placeholders::_1, tr1::placeholders::_2, tr1::placeholders::_3);
    netconfig_.walk_diff(oldcfg, close_link_func, create_link_func, update_link_func, NULL);
    
    return;
}

void calypso_main_t::reg_app_handler( const app_handler_t& h )
{
    handler_ = h;
}

netlink_t* calypso_main_t::pick_rand_link( int group )
{
    vector<int> linkids = netconfig_.get_linkid_by_group(group);
    netlink_t* link;
    vector<netlink_t*> candidates;
    for (size_t i = 0; i < linkids.size(); ++i)
    {
        link = network_.find_link(linkids.at(i));
        if (link && link->is_established())
        {
            candidates.push_back(link);
        }
    }

    if (candidates.empty())
    {
        return NULL;
    }

    return candidates[nrand(0, candidates.size()-1)];
}

void* _app_thread_main(void* args)
{
    app_thread_context_t* ctx = (app_thread_context_t*)args;
    int handle_msg_num;
    int ret;
    msgpack_context_t msgctx;
    int msg_buf_size = 1024;    // init buf size
    char *msg_buf = new char[msg_buf_size];
    if (NULL == msg_buf)
    {
        C_FATAL("create msg buf for app thread failed %p", msg_buf);
        return NULL;
    }

    int clen;
    while (ctx->th_status_ != app_thread_context_t::app_stop)
    {
        // app tick
        ctx->handler_->handle_tick_(ctx->app_inst_);

        handle_msg_num = 0;
        while (true)
        {
            // mem structure [msgctx][data..]
            clen = ctx->in_->get_consume_len();
            if (clen < 0)
            {
                C_FATAL("get_consume_len ret %d, maybe broken, ABORT NOW!", clen);
                abort();
                break;
            }

            if (msg_buf_size < clen)
            {
                // extend
                char* newbuf = new char[clen];
                if (NULL == newbuf)
                {
                    C_FATAL("no more memory for this msgpack(len:%d), SKIP it", clen);
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
                C_FATAL("consume msg queue failed ret %d, maybe broken, ABORT NOW!", ret);
                abort();
                break;
            }
            else if (ret > 0)
            {
                C_TRACE("recv msgpack from mainthread, len:%d", clen);
                memcpy(&msgctx, msg_buf, sizeof(msgctx));
                ret = ctx->handler_->handle_msgpack_(ctx->app_inst_, &msgctx, &msg_buf[sizeof(msgctx)], clen - sizeof(msgctx));
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
