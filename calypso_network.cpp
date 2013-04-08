#include "calypso_network.h"
#include <errno.h>
#include <stdio.h>

using namespace std;
using namespace std::tr1;

calypso_network_t::calypso_network_t()
{
    epfd_ = -1;
    link_list_ = NULL;
    fd2idx_ = NULL;
    max_fired_num_ = 0;
    fired_events_ = NULL;
}

calypso_network_t::~calypso_network_t()
{
    if (link_list_)
    {
        delete link_list_;
        link_list_ = NULL;
    }

    if (fd2idx_)
    {
        delete []fd2idx_;
        fd2idx_ = NULL;
    }

    if (fired_events_)
    {
        delete []fired_events_;
        fired_events_ = NULL;
    }

    close(epfd_);
}

int calypso_network_t::init( int fd_capacity, int max_fired_num, dynamic_allocator_t* allocator )
{
    empty_list_flag(used_list_);
    empty_list_flag(error_list_);

    epfd_ = epoll_create(fd_capacity);
    if (epfd_ < 0)
    {
        C_FATAL("epoll_create(%d) failed, errno %d", fd_capacity, errno);
        return -1;
    }

    link_list_ = new linked_list_t<netlink_t>(fd_capacity);
    if (NULL == link_list_)
    {
        C_FATAL("new linked_list_t<netlink_t>(%d) ret NULL", fd_capacity);
        return -1;
    }

    fd2idx_ = new int[fd_capacity];
    if (NULL == fd2idx_)
    {
        C_FATAL("create fd2idx[%d] ret NULL", fd_capacity);
        return -1;
    }

    for (int i = 0; i < fd_capacity; ++i)
    {
        fd2idx_[i] = -1;
    }

    max_fired_num_ = max_fired_num;
    fired_events_ = new epoll_event[max_fired_num_];
    if (NULL == fired_events_)
    {
        C_FATAL("create epoll_event[%d] ret NULL", max_fired_num_);
        return -1;
    }

    linked_list_t<netlink_t>::walk_list_callback callback = bind(&calypso_network_t::init_one_link, this, placeholders::_1, placeholders::_2, placeholders::_3);
    link_list_->walk_list((void*)allocator, callback);
    return 0;
}

int calypso_network_t::wait(onevent_callback callback, void* up)
{
    netlink_t::refresh_nowtime();

    int event_num = epoll_wait(epfd_, fired_events_, max_fired_num_, 0);
    if (event_num < 0)
    {
        // ERROR
        C_ERROR("epoll_wait failed, ret %d, errno %d", event_num, errno);
        return -1;
    }

    int envoke_num = 0;
    int ret;
    netlink_t* link;
    unsigned int events;
    int link_idx;
    char log_ndc[128];
    bool sock_err;
    bool cancel_epollout;
    epoll_event epevent;

    for (int i = 0; i < event_num; ++i)
    {
        sock_err = false;
        cancel_epollout = false;
        events = 0;
        link_idx = -1;
        link = get_link(fired_events_[i].data.fd, link_idx);
        snprintf(log_ndc, sizeof(log_ndc), "fd:%d,link_idx:%d", fired_events_[i].data.fd, link_idx);
        C_CLEAR_NDC();
        C_PUSH_NDC(log_ndc);
        if (NULL == link)
        {
            // ERROR
            C_ERROR("find no netlink by fd %d", fired_events_[i].data.fd);
            continue;
        }

        if (fired_events_[i].events & (EPOLLERR | EPOLLHUP))
        {
            events |= error_event;
            int sock_err = link->get_sock_error();
            C_ERROR("netlink encount error, maybe reset or timeout? sockerr is %d", sock_err);
            sock_err = true;
        }

        if (fired_events_[i].events & EPOLLIN)
        {
            update_used_list(link_idx);
            if (link->get_link_type() == netlink_t::server_link)
            {
                // NOTE:调用回调函数的参数是accept下来的link
                link_idx = accept_link(*link);
                link = link_list_->get(link_idx);
                // accept出错暂不处理
                if (NULL == link) continue;
                events |= newlink_event;
            }
            else
            {
                events |= data_arrival_event;
            }

            // link_idx
        }

        if (fired_events_[i].events & EPOLLOUT)
        {
            update_used_list(link_idx);
            if (link->is_connecting())
            {
                events |= connect_event;
                ret = link->get_sock_error();
                if (ret)
                {
                    C_ERROR("connecting sock encount error %d, close it", ret);
                    link->close();
                    sock_err = true;
                }
                else
                {
                    C_DEBUG("connecting sock %d handshake succ!", fired_events_[i].data.fd);
                    ret = link->set_established();
                    if (ret < 0)
                    {
                        C_ERROR("link set est failed, ret %d\n", ret);
                        sock_err = true;
                    }
                }
            }
            else
            {
                ret = link->send(NULL, 0);
                if (ret < 0)
                {
                    C_ERROR("link send remaining data failed ret %d", ret);
                    sock_err = true;
                }
                else if (!link->has_data_in_sendbuf())
                {
                    cancel_epollout = true;
                }
            }
        }

        // NOTE:callback里只能调用send/recv/close，不要引起除了close以外的状态变化，否则update_list时link_idx可能已经在errorlist中了
        ret = callback(link_idx, *link, events, up);
        if (ret > 0)
        {
            update_used_list(link_idx);
        }

        ++envoke_num;

        // 取消监听/监听epollout
        if (!link->is_closed() && !sock_err && (cancel_epollout | link->has_data_in_sendbuf()))
        {
            memset(&epevent, 0, sizeof(epevent));
            epevent.data.fd = link->getfd();
            
            if (cancel_epollout && !link->has_data_in_sendbuf())
            {
                // cancel
                epevent.events = EPOLLIN | EPOLLERR | EPOLLHUP;
            }

            if (link->has_data_in_sendbuf() && !cancel_epollout)
            {
                // add
                epevent.events = EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLOUT;
            }

            ret = epoll_ctl(epfd_, EPOLL_CTL_MOD, link->getfd(), &epevent);
            if (ret < 0)
            {
                C_ERROR("add connecting fd %d to epoll failed, errno %d", link->getfd(), errno);
                sock_err = true;
            }
        }

        if (link->is_closed() || sock_err)
        {
            if (netlink_t::accept_link == link->get_link_type())
            {
                recycle_used_link(*link);
            }
            else
            {
                C_WARN("netlink(fd:%d) closed or error, now move it to error list!", fired_events_[i].data.fd);
                move_link_to_error_list(*link);
            }
        }
    }

    return envoke_num;
}

netlink_t* calypso_network_t::get_link( int fd, int& idx )
{
    if (fd < 0 || fd >= link_list_->get_length())
    {
        return NULL;
    }

    idx = fd2idx_[fd];
    return link_list_->get(idx);
}

int calypso_network_t::accept_link( const netlink_t& parent )
{
    netlink_t* newlink = NULL;
    bool accept_succ = false;
    int link_idx = -1;

    do 
    {
        int ret = link_list_->append(used_list_);
        if (ret < 0)
        {
            C_FATAL("netlink list append to used list failed! ret %d", ret);
            parent.accept(NULL);
            break;
        }

        link_idx = used_list_.tail_;
        newlink = link_list_->get(link_idx);
        ret = parent.accept(newlink);
        if (ret < 0)
        {
            C_ERROR("parent(fd:%d) accept failed, ret %d", parent.getfd(), ret);
            break;
        }

        if (newlink->getfd() >= link_list_->get_length())
        {
            C_ERROR("accepted fd(%d) too big for me. MUST less than %d", newlink->getfd(), link_list_->get_length());
            break;
        }

        fd2idx_[newlink->getfd()] = link_idx;
        accept_succ = true;
    } while (false);

    if (!accept_succ)
    {
        if (newlink) recycle_used_link(*newlink);
        return -1;
    }

    return link_idx;
}

int calypso_network_t::recycle_used_link( netlink_t& link )
{
    int idx = link_list_->get_idx(&link);
    if (idx < 0)
    {
        // FATAL
        C_FATAL("netlink list get_idx by %p failed, invalid address?", &link);
        return -1;
    }

    link.close();
    int ret = link_list_->move_before(used_list_, idx, used_list_.head_);
    if (ret < 0)
    {
        C_FATAL("netlink list move_before failed (head:%d, idx:%d), ret:%d", used_list_.head_, idx, ret);
        return -1;
    }

    ret = link_list_->remove(used_list_);
    if (ret < 0)
    {
        // FATAL
        C_FATAL("netlink list remove used head(%d) failed, ret %d", used_list_.head_, ret);
        return -1;
    }

    return 0;
}

int calypso_network_t::init_one_link( int idx, netlink_t& node, void* up )
{
    node.reg_allocator(*(dynamic_allocator_t*)up);
    node.reg_status_change_callback(bind(&calypso_network_t::on_link_status_change, this, placeholders::_1));
    return idx;
}

int calypso_network_t::on_link_status_change( netlink_t& link )
{
    // NOTE: 仅操作epoll
    int ret;
    int link_idx = link_list_->get_idx(&link);
    if (link_idx < 0 || link.getfd() >= link_list_->get_length())
    {
        C_FATAL("bad linkidx(%d) or over-range fd %d", link_idx, link.getfd());
        return -1;
    }

    epoll_event epevent;
    memset(&epevent, 0, sizeof(epevent));
    epevent.data.fd = link.getfd();
    epevent.events = EPOLLIN | EPOLLERR | EPOLLHUP;

    switch (link.get_status())
    {
    case netlink_t::nsc_established:
        ret = epoll_ctl(epfd_, EPOLL_CTL_MOD, link.getfd(), &epevent);
        if (ret < 0 && ENOENT == errno)
        {
            // existed
            ret = epoll_ctl(epfd_, EPOLL_CTL_ADD, link.getfd(), &epevent);
        }

        if (ret < 0)
        {
            C_ERROR("add established fd %d to epoll failed, errno %d", link.getfd(), errno);
            return -1;
        }

        fd2idx_[link.getfd()] = link_idx;
        C_DEBUG("add established fd %d to epoll succ", link.getfd());
        break;
    case netlink_t::nsc_connecting:
        // 一次性事件
        epevent.events = EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLOUT | EPOLLONESHOT;
        ret = epoll_ctl(epfd_, EPOLL_CTL_ADD, link.getfd(), &epevent);
        if (ret < 0)
        {
            C_ERROR("add connecting fd %d to epoll failed, errno %d", link.getfd(), errno);
            return -1;
        }

        fd2idx_[link.getfd()] = link_idx;
        C_DEBUG("add connecting fd %d to epoll succ", link.getfd());
        break;
    case netlink_t::nsc_listening:
        ret = epoll_ctl(epfd_, EPOLL_CTL_ADD, link.getfd(), &epevent);
        if (ret < 0)
        {
            C_ERROR("add listening fd %d to epoll failed, errno %d", link.getfd(), errno);
            return -1;
        }

        fd2idx_[link.getfd()] = link_idx;
        C_DEBUG("add listening fd %d to epoll succ", link.getfd());
        break;
    case netlink_t::nsc_closed:
        if (link.getfd() >= 0)
        {
            ret = epoll_ctl(epfd_, EPOLL_CTL_DEL, link.getfd(), NULL);
            C_DEBUG("remove closing fd %d from epoll ret %d", link.getfd(), ret);
            fd2idx_[link.getfd()] = -1;
        }
        break;
    default:
        break;
    }

    return 0;
}

int calypso_network_t::move_link_to_error_list( netlink_t& link )
{
    // delete fd from epoll
    if (link.getfd() >= 0)
    {
        epoll_ctl(epfd_, EPOLL_CTL_DEL, link.getfd(), NULL);
    }

    // move from used to error
    int link_idx = link_list_->get_idx(&link);
    C_DEBUG("move link[%d] fd:%d to error list", link_idx, link.getfd());

    if (link_idx < 0)
    {
        C_FATAL("netlink list getidx(%p) ret %d, invalid address?", &link, link_idx);
        return -1;
    }

    int ret = link_list_->move_before(used_list_, link_idx, used_list_.head_);
    if (ret < 0)
    {
        C_FATAL("netlink list move_before failed (head:%d, idx:%d), ret %d", used_list_.head_, link_idx, ret);
        return -1;
    }

    ret = link_list_->move(used_list_, error_list_);
    if (ret < 0)
    {
        C_FATAL("move netlink from used to error failed, ret %d", ret);
        return -1;
    }

    return 0;
}

int calypso_network_t::recover_one_link( int idx, netlink_t& node, void* up )
{
    int ret;
    if (node.get_link_type() == netlink_t::accept_link)
    {
        recycle_used_link(node);
        return idx;
    }

    ret = node.recover();
    if (ret < 0)
    {
        C_ERROR("netlink[%d] recover failed, ret %d", idx, ret);
        if (node.getfd() >= 0)
        {
            epoll_ctl(epfd_, EPOLL_CTL_DEL, node.getfd(), NULL);
        }

        return idx;
    }

    int next_idx = link_list_->get_next(&node);
    // move link from error to used list
    ret = link_list_->swap(error_list_, error_list_.head_, idx);
    if (ret < 0)
    {
        C_FATAL("netlink list swap failed (head:%d, idx:%d), ret %d", error_list_.head_, idx, ret);
        return next_idx;
    }

    ret = link_list_->move(error_list_, used_list_);
    if (ret < 0)
    {
        C_FATAL("move netlink[%d] from error to used failed, ret %d", idx, ret);
        return next_idx;
    }

    return next_idx;
}

void calypso_network_t::recover( int max_recover_num )
{
    linked_list_t<netlink_t>::walk_list_callback recover_func = bind(&calypso_network_t::recover_one_link, this, placeholders::_1, placeholders::_2, placeholders::_3);
    link_list_->walk_list(max_recover_num, error_list_, NULL, recover_func);
}

void calypso_network_t::update_used_list( int link_idx )
{
    int ret = link_list_->move_after(used_list_, link_idx, used_list_.tail_);
    if (ret < 0)
    {
        C_FATAL("netlink list move %d after %d failed, ret %d", 
            link_idx, used_list_.tail_, ret);
    }
}

struct check_link_opt_t
{
    int max_idle_sec_;
    int conn_timeout_sec_;
};

void calypso_network_t::check_idle_netlink( int max_check_num, int max_idle_sec, int connect_timeout )
{
    check_link_opt_t opt;
    opt.max_idle_sec_ = max_idle_sec;
    opt.conn_timeout_sec_ = connect_timeout;
    linked_list_t<netlink_t>::walk_list_callback check_func = bind(&calypso_network_t::check_one_link, this, placeholders::_1, placeholders::_2, placeholders::_3);
    link_list_->walk_list(max_check_num, used_list_, &opt, check_func);
}

int calypso_network_t::check_one_link( int idx, netlink_t& node, void* up )
{
    int next_idx = link_list_->get_next(&node);
    check_link_opt_t* opt = (check_link_opt_t*)up;
    int sec_diff = nowtime_ - node.get_last_active_time();
    if (node.get_link_type() != netlink_t::server_link)
    {
        if ((node.is_connecting() && sec_diff >= opt->conn_timeout_sec_)
            || sec_diff >= opt->max_idle_sec_)
        {
            // connect超时或者idle太久，扔到error队列里重启
            move_link_to_error_list(node);
        }
    }

    return next_idx;
}

int calypso_network_t::create_link( const netlink_config_t::config_item_t& config )
{
    int ret = link_list_->append(used_list_);
    if (ret < 0)
    {
        C_FATAL("create new link failed, ret %d, no more links?", ret);
        return -1;
    }

    int create_idx = used_list_.tail_;
    netlink_t* link = link_list_->get(used_list_.tail_);
    netlink_t::link_opt_t opt;
    memset(&opt, 0, sizeof(opt));
    if (0 == strcmp("listen", config.type_))
    {
        opt.ltype_ = netlink_t::server_link;
        if (config.reuse_addr_) opt.flag_ |= netlink_t::lf_reuseaddr;
    }
    else
    {
        opt.ltype_ = netlink_t::client_link;
        if (config.keep_alive_) opt.flag_ |= netlink_t::lf_keepalive;
    }

    opt.flag_ = 0;
    opt.sys_rcvbuf_size_ = config.sys_recv_buffer_;
    opt.sys_sndbuf_size_ = config.sys_send_buffer_;
    opt.usr_rcvbuf_size_ = config.usr_recv_buffer_;
    opt.usr_sndbuf_size_ = config.usr_send_buffer_;

    bool succ = false;
    do 
    {
        ret = link->init(opt);
        if (ret < 0)
        {
            C_ERROR("init link %s(%s:%d) failed, ret %d", config.type_, config.ip_, config.port_, ret);
            break;
        }

        if (config.bind_ip_[0] != '\0' || config.bind_port_ > 0)
        {
            ret = link->bind(config.bind_ip_, config.bind_port_);
            if (ret < 0)
            {
                C_ERROR("bind link %s(%s:%d) failed, ret %d", config.type_, config.bind_ip_, config.bind_port_, ret);
                break;
            }
        }

        if (netlink_t::server_link == link->get_link_type())
        {
            ret = link->listen(config.back_log_);
            if (ret < 0)
            {
                C_ERROR("listen (%s:%d) failed, ret %d", config.bind_ip_, config.bind_port_, ret);
                break;
            }

            C_DEBUG("listen (%s:%d) succ", config.bind_ip_, config.bind_port_);
        }
        else
        {
            ret = link->connect(config.ip_, config.port_);
            if (ret < 0)
            {
                C_ERROR("connect (%s:%d) failed, ret %d\n", config.ip_, config.port_, ret);
                break;
            }
        }

        succ = true;
    } while (false);

    // close
    if (!succ)
    {
        move_link_to_error_list(*link);
    }

    return create_idx;
}

int calypso_network_t::close_link( int idx )
{
    netlink_t* link = link_list_->get(idx);
    if (NULL == link)
    {
        printf("find no link[%d] when closing", idx);
        return -1;
    }

    return recycle_used_link(*link);
}
