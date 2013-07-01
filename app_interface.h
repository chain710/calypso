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

    struct msgpack_context_t
    {
        int magic_;     // magic number(0)
        int link_ctx_;  // �ش����������߳��ҵ�����·
        int link_fd_;   // fd
        unsigned int flag_; // ���ܱ�ʶ see msgpack_flag_t
        int link_type_;   // ��·���� see netlink_t linktype
        sockaddr_in remote_;    // �Զ˵�ַ
        sockaddr_in local_;     // ������ַ
    };

    // ��ʼ��app
    typedef void* (*app_initialize_func_t)(void* container);
    // ����
    typedef void (*app_finalize_func_t)(void* app_inst);
    // ����tick
    typedef void (*app_handle_tick_func_t)(void* app_inst);
    // ����buf�е�package��С, ע�⣺get_msgpack_size_��ֱ�ӱ����̵߳��õģ�ʹ��app_instָ����Σ�գ�
    typedef int (*app_get_msgpack_size_func_t)(void* app_inst, const msgpack_context_t* ctx, const char*, size_t);
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

    //////////////////////////////////////////////////////////////////////////
    // app ����
    //////////////////////////////////////////////////////////////////////////
    // ����ctx�����Ϣ�ҵ���Ӧ��·����������
    int calypso_send_msgpack_by_ctx(void* main_inst, const msgpack_context_t* ctx, const char* data, size_t len);

    // ѡ��ָ��group���һ����·����������
    int calypso_send_msgpack_by_group(void* main_inst, int group, const char* data, size_t len);

    // �㲥��group���������·
    int calypso_broadcast_msgpack_by_group(void* main_inst, int group, const char* data, size_t len);

    bool calypso_need_reload(time_t last);
#ifdef __cplusplus
}
#endif

#endif
