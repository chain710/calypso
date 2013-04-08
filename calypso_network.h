#ifndef _CALYPSO_NETWORK_H_
#define _CALYPSO_NETWORK_H_

#include "linked_list.h"
#include "netlink.h"
#include "netlink_config.h"
#include "log_interface.h"
#include "allocator.h"
#include <tr1/functional>
#include <sys/epoll.h>

enum net_event_t
{
    connect_event = 0x00000001,
    data_arrival_event = 0x00000002,
    error_event = 0x00000004,
    newlink_event = 0x00000008,
};

class calypso_network_t
{
public:
    typedef std::tr1::function<int (int, netlink_t&, unsigned int evt, void*)> onevent_callback;

    calypso_network_t();
    virtual ~calypso_network_t();
    int init(int fd_capacity, int max_fired_num, dynamic_allocator_t* allocator);
    // 等待epoll事件，返回事件个数
    int wait(onevent_callback callback, void* up);
    void recover(int max_recover_num);
    // NOTE: 检查connect超时用定时器实现。检查client/accept链路是否活跃，配置统一的值，而不是每个链路一个配置！
    void check_idle_netlink(int max_check_num, int max_idle_sec, int connect_timeout);
    // 按配置创建/关闭链路，返回创建netlink的下标
    int create_link(const netlink_config_t::config_item_t& config);
    int close_link(int idx);
    void refresh_nowtime(time_t t) { nowtime_ = t; }
    netlink_t* find_link(int idx) { return link_list_->get(idx); }
private:
    calypso_network_t(const calypso_network_t&) {}
    netlink_t* get_link(int fd, int& idx);
    int accept_link(const netlink_t& parent);
    int recycle_used_link(netlink_t& link);
    int init_one_link(int idx, netlink_t& node, void* up);
    int recover_one_link(int idx, netlink_t& node, void* up);
    int check_one_link(int idx, netlink_t& node, void* up);
    int on_link_status_change(netlink_t& link);
    int move_link_to_error_list(netlink_t& link);
    void update_used_list(int link_idx);

    int epfd_;
    // 网络连接链表
    linked_list_t<netlink_t>* link_list_;
    // 记录已使用的netlink(tail最新，head最旧)
    linked_list_flag_t used_list_;
    // 出现问题的netlink
    linked_list_flag_t error_list_;
    // fd到list idx转换
    int *fd2idx_;
    // fired events
    int max_fired_num_;
    epoll_event* fired_events_;
    time_t nowtime_;
};

#endif
