#ifndef _CALYPSO_H_
#define _CALYPSO_H_
#include "allocator.h"
#include "calypso_network.h"
#include "netlink_config.h"
#include "calypso_bootstrap_config.h"
#include "calypso_runtime_config.h"
#include "ring_queue.h"
#include "app_interface.h"
#include <pthread.h>

struct app_thread_context_t
{
    enum app_thread_status_t
    {
        app_running = 1,
        app_stop = 2,
        app_deprecated = 3, // �ȴ�stop�У������߳�
    };

    app_handler_t* handler_;
    void* app_inst_;
    ring_queue_t* in_;  // ������Ϣ���� dispatch->app
    ring_queue_t* out_; // �����Ϣ���� app->dispatch
    pthread_t th_;
    pthread_attr_t th_attr_;
    pthread_mutex_t th_mutex_;
    pthread_cond_t th_cond_;    // NOT USED FOR NOW
    int th_status_;
    time_t deprecated_time_;    // ����deprecated״̬��ʱ��
    time_t last_busy_time_;     // �ϴδ��������Ϣ���з�0��ʱ��
    unsigned fatal_:1;
    unsigned res_:31;
};

const int CALYPSO_APP_THREAD_SWITCH_NUM = 2;

class calypso_main_t
{
public:
    calypso_main_t();
    virtual ~calypso_main_t();

    int initialize(const char* bootstrap_path);
    void reg_app_handler(const app_handler_t& h);
    void run();
    void cleanup();

    // app interface
    int send_by_group(int group, const char* data, size_t len);
    void broadcast_by_group(int group, const char* data, size_t len);
    int send_by_context(msgpack_context_t ctx, const char* data, size_t len);
    app_thread_context_t* get_app_ctx() { return &app_ctx_[running_app_]; }
private:
    // deny copy-cons
    calypso_main_t(const calypso_main_t&) {}
    // ����Ӧ���߳�
    int create_appthread(app_thread_context_t& thread_ctx);
    // ֹͣӦ���߳�
    void stop_appthread(app_thread_context_t& thread_ctx);
    // ����Ӧ���߳�
    int restart_app();
    int restart_appthread(app_thread_context_t& thread_ctx, app_thread_context_t& deprecated_ctx);
    // ����netlink�±�
    int create_link(int idx, const netlink_config_t::config_item_t& config, void* up);
    // �ر���·
    int close_link(int idx, const netlink_config_t::config_item_t& config, void* up);
    // �����¼��ص�
    int on_net_event(int link_idx, netlink_t&, unsigned int evt, void*);
    // ����Ϣ���͸�Ӧ���߳�, simply send ctx if data==null
    int dispatch_msg_to_app(app_thread_context_t& ctx, const msgpack_context_t& msgctx, const char* data, size_t len, const char* extrabuf);
    // ��Ӧ���̻߳�ȡ��Ϣ��ת��
    int process_appthread_msg(app_thread_context_t& thread_ctx);
    // ��������߳�
    void check_deprecated_threads(app_thread_context_t& thread_ctx);
    // do staff like dispatching msg, checking signal .etc
    void main();

    void reload_config();

    dynamic_allocator_t allocator_;
    fixed_size_allocator_t* subs_;
    calypso_network_t network_;
    netlink_config_t netconfig_;

    app_thread_context_t app_ctx_[CALYPSO_APP_THREAD_SWITCH_NUM];
    int running_app_;
    void* app_dl_handler_;

    app_handler_t handler_;
    char* out_msg_buf_;
    int out_msg_buf_size_;
    time_t nowtime_;
    time_t last_reload_config_;

    calypso_bootstrap_config_t bootstrap_config_;
    calypso_runtime_config_t runtime_config_;
};


#endif
