#ifndef _APP_INTERFACE_H_
#define _APP_INTERFACE_H_
#include <netinet/in.h>
#ifdef __cplusplus
extern "C"
{
#endif
    // TODO: dispatch�Ƿ��б�Ҫ֪ͨworker��·�رգ����֪ͨ�Ļ����뱣֤��1.��ͬfd�����䵽��ͬworker������ʱ��ᵼ������
    enum msgpack_flag_t
    {
        mpf_closed_by_peer = 0x00000001,
        mpf_new_connection = 0x00000002,
        mpf_close_link = 0x00000004,    // worker->dispatch �رն�Ӧ��·
    };

    enum transfer_type_t
    {
        tt_broadcast_group = -2,   // �㲥group
        tt_send_group = -1,    // �����ѡgroup�µ�һ����·����
    };

    struct msgpack_context_t
    {
        int magic_;     // magic number(0)
        int link_ctx_;  // �ش����������߳��ҵ�����·
        int link_fd_;   // >=0 fd
        unsigned int flag_; // ���ܱ�ʶ see msgpack_flag_t
        short link_type_;   // ��·���� see netlink_t linktype
        short extrabuf_len_;   // app_get_msgpack_size_func_tʱ�����Ķ�������
        sockaddr_in remote_;    // �Զ˵�ַ
        sockaddr_in local_;     // ������ַ
    };

    struct app_init_option
    {
        int th_idx_;
        void* msg_queue_;
    };

    // ��ʼ��app
    typedef void* (*app_initialize_func_t)(app_init_option opt);
    // ����
    typedef void (*app_finalize_func_t)(void* app_inst);
    // ����tick
    typedef void (*app_handle_tick_func_t)(void* app_inst);
    /*
     *	����buf�е�package��С, 0=��δ����>0�����ȣ�<0����������
     *  NOTE��app_get_msgpack_size_func_t called directly by main thread, so no appinst ptr provided
     *  OUT: extra_buf, extrabuflen������msgpackʱ���������ݣ��������msgpackһ���͸�app�߳�[�ṹ��ctx,extrabuf,msgpack]
     */
    typedef int (*app_get_msgpack_size_func_t)(const msgpack_context_t* ctx, const char*, size_t, char* extra_buf, size_t*);
    // ����һ��package
    typedef int (*app_handle_msgpack_func_t)(void* app_inst, const msgpack_context_t* ctx, const char*, size_t);

    struct app_handler_t
    {
        app_initialize_func_t init_;
        app_finalize_func_t fina_;
        app_handle_tick_func_t handle_tick_;
        app_get_msgpack_size_func_t get_msgpack_size_;
        app_handle_msgpack_func_t handle_msgpack_;
    };

    //////////////////////////////////////////////////////////////////////////
    // app ʵ��
    //////////////////////////////////////////////////////////////////////////
    // ����apphandler
    app_handler_t get_app_handler();
    // ȫ�ֳ�ʼ���������ʼ��appȫ�ֱ������̴߳�ȡȫ�ֱ���ʱ�����м���
    int app_global_init();
    // ȫ��reload
    void app_global_reload();
    // ����ȫ�ֱ���
    void app_global_fina();

    //////////////////////////////////////////////////////////////////////////
    // app ����
    //////////////////////////////////////////////////////////////////////////
    // ����ctx�����Ϣ�ҵ���Ӧ��·����������
    int calypso_send_msgpack_by_ctx(void* queue, const msgpack_context_t* ctx, const char* data, size_t len);

    // ѡ��ָ��group���һ����·����������
    int calypso_send_msgpack_by_group(void* queue, int group, const char* data, size_t len);

    // �㲥��group���������·
    int calypso_broadcast_msgpack_by_group(void* queue, int group, const char* data, size_t len);

    bool calypso_need_reload(time_t last);
#ifdef __cplusplus
}
#endif

#endif

