#ifndef _CALYPSO_NETLINK_H_
#define _CALYPSO_NETLINK_H_

#include "allocator.h"
#include <arpa/inet.h>
#include <tr1/functional>
#include <time.h>

class netlink_t
{
public:
    struct link_opt_t
    {
        // see link_type_t
        int ltype_;
        int usr_sndbuf_size_;
        int usr_rcvbuf_size_;
        int sys_sndbuf_size_;
        int sys_rcvbuf_size_;
        unsigned int flag_;
    };

    enum link_flag_t
    {
        lf_keepalive = 0x00000001,
        lf_reuseaddr = 0x00000002,
    };

    enum link_status_t
    {
        nsc_closed = 0, 
        nsc_established = 1,
        nsc_connecting = 2,
        nsc_listening = 3,
        nsc_open = 4,
    };

    enum link_type_t
    {
        client_link = 0,    // tcp:��������connect
        server_link = 1,    // tcp:listen
        accept_link = 2,    // tcp:accept������
    };

    struct link_buffer_t
    {
        int used_;
        char data_[0];
    };

    typedef std::tr1::function<int (netlink_t&)> on_status_change_callback;

    netlink_t();
    virtual ~netlink_t();
    void reg_allocator(dynamic_allocator_t& allocator) { allocator_ = &allocator; }
    void reg_status_change_callback(on_status_change_callback callback) { on_status_change_ = callback; }
    void set_bind_addr(const char* ip, unsigned short port);
    void set_listen_backlog(int backlog);
    void set_remote_addr(const char* ip, unsigned short port);
    // ��ʼ������������·״̬������fd
    int init(link_opt_t opt, int fd = -1);
    // �������ò���socket
    int configure();
    int accept(netlink_t* child) const;
    int bind(const char* ip, unsigned short port);
    int listen(int backlog);
    int connect(const char* ip, unsigned short port);
    int recv();
    int send(const char* buf, int len);
    // �ر���·�����ǲ��黹���棬����recover
    int close();
    // ����������ݣ��ر���·���黹���棬����recover
    int clear();
    // send/recv/accept��������close/error�ˣ����Ե���recover���½�������/�����˿ڣ�ע��fd��Ҫ���¼���epoll
    int recover();
    int getfd() const { return fd_; }
    int set_established();
    
    bool is_established() const { return status_ == nsc_established; }
    bool is_closed() const { return status_ == nsc_closed; }
    bool is_connecting() const { return status_ == nsc_connecting; }

    // ��ý��ջ���ͷָ��ͳ���
    char* get_recv_buffer(int& len);
    // ���ָ�����ȵĽ��ջ�����������ʵ���������, len=-1��ʾȫ��
    int pop_recv_buffer(int len);

    int get_status() const { return status_; }
    int get_link_type() const { return opt_.ltype_; }
    // utility
    const char* get_local_addr_str(char* buf, int size) const;
    const char* get_remote_addr_str(char* buf, int size) const;
    sockaddr_in get_local_addr() const { return local_addr_; }
    sockaddr_in get_remote_addr() const { return remote_addr_; }

    link_opt_t get_opt() const { return opt_; }
    int get_sock_error() const;
    time_t get_last_active_time() const { return last_active_time_; }
    time_t get_last_recover_time() const { return last_recover_time_; }
    // �Ƿ����������û����ͻ������д�����
    bool has_data_in_sendbuf() const { return send_buf_->used_ > 0; }
    static void refresh_nowtime(time_t t) { now_time_ = t; }
private:
    // deny copy-cons
    netlink_t(const netlink_t& c) {}
    int mod_status(int status);
    int setup();
    int add_fd_flag(int flags);
    int set_sock_buffer_size(int snd, int rcv);
    int do_connect();
    void do_close();
    int copy_data_to_send_buffer(const char* buffer, int len);
    int fd_;
    short status_;
    sockaddr_in local_addr_;
    sockaddr_in remote_addr_;
    dynamic_allocator_t* allocator_;
    // recv�����������ݶ��ȷŵ�������
    link_buffer_t* recv_buf_;
    // send����eagainʱ����
    link_buffer_t* send_buf_;

    time_t last_active_time_;
    time_t last_recover_time_;
    link_opt_t opt_;
    int listen_backlog_;

    // ״̬�ı�ʱ�ص�
    on_status_change_callback on_status_change_;

    static time_t now_time_;
};

#endif
