#include "app_interface.h"
#include "calypso.h"
#include "calypso_signal.h"

int calypso_send_msgpack_by_ctx( void* queue, const msgpack_context_t* ctx, const char* data, size_t len )
{
    if (NULL == data) len = 0;

    ring_queue_t* out = (ring_queue_t*)queue;
    int ret = out->produce_reserve(len+sizeof(msgpack_context_t));
    if (ret < 0) return -1;
    ret = out->produce_append((char*)ctx, sizeof(msgpack_context_t));
    if (ret < 0) return -1;
    if (data && len > 0)
    {
        ret = out->produce_append(data, len);
        if (ret < 0) return -1;
    }

    ret = out->produce_append(NULL, 0);
    if (ret < 0) return -1;

    return 0;
}

int calypso_send_msgpack_by_group( void* queue, int group, const char* data, size_t len )
{
    if (NULL == data) len = 0;
    msgpack_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.link_ctx_ = group;
    ctx.link_fd_ = tt_send_group;
    ring_queue_t* out = (ring_queue_t*)queue;
    int ret = out->produce_reserve(len+sizeof(ctx));
    if (ret < 0) return -1;
    ret = out->produce_append((char*)&ctx, sizeof(ctx));
    if (ret < 0) return -1;
    if (data && len > 0)
    {
        ret = out->produce_append(data, len);
        if (ret < 0) return -1;
    }

    ret = out->produce_append(NULL, 0);
    if (ret < 0) return -1;

    return 0;
}

int calypso_broadcast_msgpack_by_group( void* queue, int group, const char* data, size_t len )
{
    if (NULL == data) len = 0;
    msgpack_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.link_ctx_ = group;
    ctx.link_fd_ = tt_broadcast_group;
    ring_queue_t* out = (ring_queue_t*)queue;
    int ret = out->produce_reserve(len+sizeof(ctx));
    if (ret < 0) return -1;
    ret = out->produce_append((char*)&ctx, sizeof(ctx));
    if (ret < 0) return -1;
    if (data && len > 0)
    {
        ret = out->produce_append(data, len);
        if (ret < 0) return -1;
    }

    ret = out->produce_append(NULL, 0);
    if (ret < 0) return -1;

    return 0;
}

bool calypso_need_reload( time_t last )
{
    return need_reload(last);
}
