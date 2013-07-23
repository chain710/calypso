#include "calypso_network.h"
#include <errno.h>
#include <stdio.h>

using namespace std;
using namespace std::tr1;

struct recover_link_opt_t
{
    int min_recover_interval_;
};

calypso_network_t::calypso_network_t()
{
    epfd_ = -1;
    link_list_ = NULL;
    fd2idx_ = NULL;
    max_fired_num_ = 0;
    fired_events_ = NULL;
    refresh_nowtime(time(NULL));
}

calypso_network_t::~calypso_network_t()
{
    fina();
}

int calypso_network_t::init( int fd_capacity, int max_fired_num, dynamic_allocator_t* allocator )
{
    epfd_ = epoll_create(fd_capacity);
    if (epfd_ < 0)
    {
        C_FATAL("epoll_create(%d) failed, errno %d", fd_capacity, errno);
        return -1;
    }

    link_list_ = new linked_list_t<netlink_t>(fd_capacity, lerror);
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

int calypso_network_t::wait(const onevent_callback& callback, void* up)
{
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
    char addr_str[32];
    bool link_err;
    bool cancel_epollout;
    epoll_event epevent;

    for (int i = 0; i < event_num; ++i)
    {
        link_err = false;
        cancel_epollout = false;
        events = 0;
        link_idx = -1;
        link = get_link(fired_events_[i].data.fd, link_idx);
        if (NULL == link)
        {
            C_ERROR("find no netlink by fd %d", fired_events_[i].data.fd);
            continue;
        }

        if (fired_events_[i].events & (EPOLLERR | EPOLLHUP))
        {
            events |= error_event;
            int sock_errno = link->get_sock_error();
            C_ERROR("netlink[%d] fd(%d) encount error, maybe reset or timeout? sockerr is %d", 
                link_idx, link->getfd(), sock_errno);
            link_err = true;
        }

        if (fired_events_[i].events & EPOLLIN)
        {
            update_used_list(link_idx);
            if (link->get_link_type() == netlink_t::server_link)
            {
                netlink_t* parent = link;
                C_DEBUG("new connection on %s", parent->get_local_addr_str(addr_str, sizeof(addr_str)));
                link_idx = accept_link(*link);
                if (link_idx < 0)
                {
                    C_ERROR("accept conn on %s failed %d", parent->get_local_addr_str(addr_str, sizeof(addr_str)), link_idx);
                    continue;
                }
                link = link_list_->get(link_idx);
                // link should not be NULL
                if (NULL == link) abort();

                C_DEBUG("new connection from %s accepted", link->get_remote_addr_str(addr_str, sizeof(addr_str)));
                events |= newlink_event;
            }
            else
            {
                events |= data_arrival_event;
            }
        }

        if (fired_events_[i].events & EPOLLOUT)
        {
            update_used_list(link_idx);
            if (link->is_connecting() && !link_err)
            {
                events |= connect_event;
                ret = link->get_sock_error();
                if (ret)
                {
                    C_ERROR("connecting sock(%s) encount error %d, close it", 
                        link->get_remote_addr_str(addr_str, sizeof(addr_str)), ret);
                    link_err = true;
                }
                else
                {
                    C_DEBUG("connecting sock(%s) fd:%d handshake succ!", 
                        link->get_remote_addr_str(addr_str, sizeof(addr_str)), fired_events_[i].data.fd);
                    ret = link->set_established();
                    if (ret < 0)
                    {
                        C_ERROR("link set est failed, ret %d\n", ret);
                        link_err = true;
                    }
                }
            }
            else if (link->has_data_in_sendbuf())
            {
                // send remaining data
                ret = link->send(NULL, 0);
                if (ret < 0)
                {
                    C_ERROR("link send remaining data failed ret %d", ret);
                    link_err = true;
                }
                else if (!link->has_data_in_sendbuf())
                {
                    // all sent out
                    cancel_epollout = true;
                }
            }
        }

        // NOTE: ONLY send/recv/close allowd in callback, otherwise link_idx might already be in errorlist when update_used_list
        ret = callback(link_idx, *link, events, up);
        if (ret > 0)
        {
            update_used_list(link_idx);
        }

        ++envoke_num;

        // cancel/add epollout
        if (!link->is_closed() && !link_err && (cancel_epollout | link->has_data_in_sendbuf()))
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
                link_err = true;
            }
        }

        if (link->is_closed() || link_err)
        {
            C_WARN("netlink(fd:%d) closed or error, shut it down!", fired_events_[i].data.fd);
            shutdown_link(link_idx);
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
        int ret = link_list_->append(lused);
        if (ret < 0)
        {
            C_FATAL("netlink list append to used list failed! ret %d", ret);
            parent.accept(NULL);
            break;
        }

        link_idx = link_list_->get_tail(lused);
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
        if (newlink) recycle_link(*newlink);
        return -1;
    }

    return link_idx;
}

int calypso_network_t::recycle_link( netlink_t& link )
{
    int idx = link_list_->get_idx(&link);
    if (idx < 0)
    {
        // FATAL
        C_FATAL("netlink list get_idx by %p failed, invalid address?", &link);
        return -1;
    }

    int lid = link_list_->get_list_id(idx);
    if (lid < 0)
    {
        C_FATAL("invalid listid %d by link[%d]", lid, idx);
        return -1;
    }

    C_TRACE("recycle link[%d]", idx);
    link.clear();
    int ret = link_list_->move_before(idx, link_list_->get_head(lid));
    if (ret < 0)
    {
        C_FATAL("netlink list(%d) move_before failed (head:%d, idx:%d), ret:%d", lid, link_list_->get_head(lid), idx, ret);
        return -1;
    }

    ret = link_list_->remove(lid);
    if (ret < 0)
    {
        // FATAL
        C_FATAL("netlink list(%d) remove used head(%d) failed, ret %d", lid, link_list_->get_head(lid), ret);
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
    // NOTE: ONLY operate epoll
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
        // one time event
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

    int ret = link_list_->move_before(link_idx, link_list_->get_head(lused));
    if (ret < 0)
    {
        C_FATAL("netlink list move_before failed (head:%d, idx:%d), ret %d", link_list_->get_head(lused), link_idx, ret);
        return -1;
    }

    ret = link_list_->move(lused, lerror);
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
        recycle_link(node);
        return idx;
    }

    // prevent recovering frequently
    recover_link_opt_t* opt = (recover_link_opt_t*)up;
    if ( opt && nowtime_ - node.get_last_recover_time() < opt->min_recover_interval_)
    {
        return idx;
    }

    ret = node.recover();
    char remote_addr_str[64];
    char local_addr_str[64];
    C_TRACE("link(r:%s, l:%s) recover ret %d", 
        node.get_remote_addr_str(remote_addr_str, sizeof(remote_addr_str)), 
        node.get_local_addr_str(local_addr_str, sizeof(local_addr_str)), ret);
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
    int lid = link_list_->get_list_id(idx);
    if (lid < 0)
    {
        C_FATAL("list id by link[%d] is %d", idx, lid);
        return -1;
    }

    if (lid == lused)
    {
        // already in used list
        C_WARN("link[%d] already in used list, no need to recover", idx);
        return next_idx;
    }

    // move link from error to used list
    ret = link_list_->swap(link_list_->get_head(lerror), idx);
    if (ret < 0)
    {
        C_FATAL("netlink list swap failed (head:%d, idx:%d), ret %d", link_list_->get_head(lerror), idx, ret);
        return next_idx;
    }

    ret = link_list_->move(lerror, lused);
    if (ret < 0)
    {
        C_FATAL("move netlink[%d] from error to used failed, ret %d", idx, ret);
        return next_idx;
    }

    return next_idx;
}

void calypso_network_t::recover( int max_recover_num, int min_recover_interval )
{
    recover_link_opt_t opt;
    opt.min_recover_interval_ = min_recover_interval;
    linked_list_t<netlink_t>::walk_list_callback recover_func = bind(&calypso_network_t::recover_one_link, this, placeholders::_1, placeholders::_2, placeholders::_3);
    link_list_->walk_list(max_recover_num, lerror, &opt, recover_func);
}

void calypso_network_t::update_used_list( int link_idx )
{
    int lid = link_list_->get_list_id(link_idx);
    if (lid != lused)
    {
        C_FATAL("expect link[%d].listid to be lused but %d", link_idx, lid);
        return;
    }

    int ret = link_list_->move_after(link_idx, link_list_->get_tail(lid));
    if (ret < 0)
    {
        C_FATAL("netlink list move %d after %d failed, ret %d", 
            link_idx, link_list_->get_tail(lid), ret);
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
    link_list_->walk_list(max_check_num, lused, &opt, check_func);
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
            // connect timeout or idle too long, move to errorlist
            C_WARN("link(idx:%d fd:%d) idle too long(%d)", idx, node.getfd(), sec_diff);
            move_link_to_error_list(node);
        }
    }

    return next_idx;
}

int calypso_network_t::create_link( const netlink_config_t::config_item_t& config )
{
    int ret = link_list_->append(lused);
    if (ret < 0)
    {
        C_FATAL("create new link failed, ret %d, no more links?", ret);
        return -1;
    }

    int create_idx = link_list_->get_tail(lused);
    netlink_t* link = link_list_->get(create_idx);
    netlink_t::link_opt_t opt = netlink_config_to_option(config);

    bool succ = false;
    do 
    {
        ret = link->init(opt);
        // set param first, in case can not recover because of failure in middle of configure
        if (config.bind_ip_[0] != '\0' || config.bind_port_ > 0)
        {
            link->set_bind_addr(config.bind_ip_, config.bind_port_);
        }
        
        if (netlink_t::server_link == link->get_link_type())
        {
            link->set_listen_backlog(config.back_log_);
        }
        else
        {
            link->set_remote_addr(config.ip_, config.port_);
        }

        if (ret < 0)
        {
            C_ERROR("init link %s(%s:%d) failed, ret %d", config.type_, config.ip_, config.port_, ret);
            break;
        }

        ret = link->configure();
        if (ret < 0)
        {
            C_ERROR("link(%s) configure failed, bind %s:%d, remote %s:%d, ret %d", 
                config.type_, config.bind_ip_, config.bind_port_, config.ip_, config.port_, ret);
            break;
        }

        succ = true;
    } while (false);

    // move to error list, waiting for recovery
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
        C_ERROR("find no link[%d] when closing", idx);
        return -1;
    }

    return recycle_link(*link);
}

int calypso_network_t::shutdown_link( int idx )
{
    netlink_t* link = link_list_->get(idx);
    if (NULL == link)
    {
        C_ERROR("find no link[%d] when shuting down", idx);
        return -1;
    }

    if (link->get_link_type() == netlink_t::accept_link)
    {
        return recycle_link(*link);
    }
    else
    {
        link->close();
        return move_link_to_error_list(*link);
    }
}

void calypso_network_t::fina()
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
    epfd_ = -1;
}

int calypso_network_t::update_link( int idx, const netlink_config_t::config_item_t& config )
{
    netlink_t::link_opt_t newopt = netlink_config_to_option(config);
    netlink_t* link = link_list_->get(idx);
    if (NULL == link)
    {
        C_ERROR("find no link by idx %d, config-network correspondence may be broken!", idx);
        return idx;
    }

    link->update_opt(newopt);
    return idx;
}

netlink_t::link_opt_t netlink_config_to_option( const netlink_config_t::config_item_t& config )
{
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

    opt.sys_rcvbuf_size_ = config.sys_recv_buffer_;
    opt.sys_sndbuf_size_ = config.sys_send_buffer_;
    opt.usr_rcvbuf_size_ = config.usr_recv_buffer_;
    opt.usr_sndbuf_size_ = config.usr_send_buffer_;
    opt.mask_ = config.mask_;

    return opt;
}
