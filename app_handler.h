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
    int link_ctx_;  // �ش����������߳��ҵ�����·
    int link_fd_;   // fd
    unsigned int flag_; // ���ܱ�ʶ see msgpack_flag_t
    int link_type_;   // ��·���� see netlink_t linktype
    sockaddr_in remote_;    // �Զ˵�ַ
    sockaddr_in local_;     // ������ַ
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
