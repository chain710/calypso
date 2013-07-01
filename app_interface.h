#ifndef _APP_INTERFACE_H_
#define _APP_INTERFACE_H_
#include <netinet/in.h>
#ifdef __cplusplus
extern "C"
{
#endif
    // TODO: dispatch是否有必要通知worker链路关闭？如果通知的话必须保证：1.相同fd被分配到相同worker，否则时序会导致问题
    enum msgpack_flag_t
    {
        mpf_closed_by_peer = 0x00000001,
        mpf_new_connection = 0x00000002,
        mpf_close_link = 0x00000004,    // worker->dispatch 关闭对应链路
    };

    struct msgpack_context_t
    {
        int magic_;     // magic number(0)
        int link_ctx_;  // 回传，方便主线程找到该链路
        int link_fd_;   // fd
        unsigned int flag_; // 功能标识 see msgpack_flag_t
        int link_type_;   // 链路类型 see netlink_t linktype
        sockaddr_in remote_;    // 对端地址
        sockaddr_in local_;     // 本机地址
    };

    // 初始化app
    typedef void* (*app_initialize_func_t)(void* container);
    // 销毁
    typedef void (*app_finalize_func_t)(void* app_inst);
    // 处理tick
    typedef void (*app_handle_tick_func_t)(void* app_inst);
    // 返回buf中的package大小, 注意：get_msgpack_size_是直接被主线程调用的，使用app_inst指针有危险！
    typedef int (*app_get_msgpack_size_func_t)(void* app_inst, const msgpack_context_t* ctx, const char*, size_t);
    // 处理一个package
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
    // app 实现
    //////////////////////////////////////////////////////////////////////////
    // 返回apphandler
    app_handler_t get_app_handler();

    //////////////////////////////////////////////////////////////////////////
    // app 调用
    //////////////////////////////////////////////////////////////////////////
    // 利用ctx里的信息找到对应链路，发送数据
    int calypso_send_msgpack_by_ctx(void* main_inst, const msgpack_context_t* ctx, const char* data, size_t len);

    // 选择指定group里的一个链路，发送数据
    int calypso_send_msgpack_by_group(void* main_inst, int group, const char* data, size_t len);

    // 广播到group里的所有链路
    int calypso_broadcast_msgpack_by_group(void* main_inst, int group, const char* data, size_t len);

    bool calypso_need_reload(time_t last);
#ifdef __cplusplus
}
#endif

#endif

