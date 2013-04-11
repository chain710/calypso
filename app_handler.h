#ifndef _APP_HANDLER_H_
#define _APP_HANDLER_H_

#include <netinet/in.h>

enum msgpack_flag_t
{
    mpf_closed_by_peer = 0x00000001,
    mpf_new_connection = 0x00000002,
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

class app_handler_t
{
public:
    virtual ~app_handler_t() {}
    virtual void handle_tick() = 0;
    virtual int get_msgpack_size(msgpack_context_t ctx, const char*, size_t) const = 0;
    virtual int handle_msgpack(msgpack_context_t ctx, const char*, size_t) = 0;
};

#endif
