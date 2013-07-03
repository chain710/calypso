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
    connect_event = 0x00000001, // connect�ɹ�
    data_arrival_event = 0x00000002,    // �����ݿɶ�
    error_event = 0x00000004,   // ����
    newlink_event = 0x00000008, // ������accept
};

class calypso_network_t
{
public:
    typedef std::tr1::function<int (int, netlink_t&, unsigned int evt, void*)> onevent_callback;

    calypso_network_t();
    virtual ~calypso_network_t();
    int init(int fd_capacity, int max_fired_num, dynamic_allocator_t* allocator);
    void fina();
    // �ȴ�epoll�¼��������¼�����
    int wait(onevent_callback callback, void* up);
    void recover(int max_recover_num, int min_recover_interval);
    // NOTE: ���connect��ʱ�ö�ʱ��ʵ�֡����client/accept��·�Ƿ��Ծ������ͳһ��ֵ��������ÿ����·һ�����ã�
    void check_idle_netlink(int max_check_num, int max_idle_sec, int connect_timeout);
    // �����ô���/�ر���·�����ش���netlink���±�
    int create_link(const netlink_config_t::config_item_t& config);
    // ���׹ر���·
    int close_link(int idx);
    // ������·option
    int update_link(int idx, const netlink_config_t::config_item_t& config);
    // �����·��accept������ֱ�ӹرգ�����رղ�����errorlist�ȴ�����
    int shutdown_link(int idx);
    void refresh_nowtime(time_t t) { nowtime_ = t; netlink_t::refresh_nowtime(t); }
    netlink_t* find_link(int idx) { return link_list_->get(idx); }
private:
    calypso_network_t(const calypso_network_t&) {}
    netlink_t* get_link(int fd, int& idx);
    int accept_link(const netlink_t& parent);
    // �ر���·��������Դ
    int recycle_link(netlink_t& link);
    int init_one_link(int idx, netlink_t& node, void* up);
    int recover_one_link(int idx, netlink_t& node, void* up);
    int check_one_link(int idx, netlink_t& node, void* up);
    int on_link_status_change(netlink_t& link);
    int move_link_to_error_list(netlink_t& link);
    void update_used_list(int link_idx);

    enum link_id_t
    {
        lfree = 0,
        lused = 1,
        lerror = 2,
    };

    int epfd_;
    // ������������
    linked_list_t<netlink_t>* link_list_;
    // fd��list idxת��
    int *fd2idx_;
    // fired events
    int max_fired_num_;
    epoll_event* fired_events_;
    time_t nowtime_;
};


// linkconfig to linkopt
netlink_t::link_opt_t netlink_config_to_option(const netlink_config_t::config_item_t& config);

#endif
