#include "app_interface.h"
#include "calypso.h"
#include "calypso_signal.h"

int calypso_send_msgpack_by_ctx( void* main_inst, const msgpack_context_t* ctx, const char* data, size_t len )
{
    if (NULL == data) len = 0;

    calypso_main_t* inst = (calypso_main_t*)main_inst;
    app_thread_context_t* app_ctx = inst->get_app_ctx();
    int ret = app_ctx->out_->produce_reserve(len+sizeof(msgpack_context_t));
    if (ret < 0) return -1;
    ret = app_ctx->out_->produce_append((char*)ctx, sizeof(msgpack_context_t));
    if (ret < 0) return -1;
    if (data && len > 0)
    {
        ret = app_ctx->out_->produce_append(data, len);
        if (ret < 0) return -1;
    }

    ret = app_ctx->out_->produce_append(NULL, 0);
    if (ret < 0) return -1;

    return 0;
}

int calypso_send_msgpack_by_group( void* main_inst, int group, const char* data, size_t len )
{
    return 0;
}

int calypso_broadcast_msgpack_by_group( void* main_inst, int group, const char* data, size_t len )
{
    return 0;
}

bool calypso_need_reload( time_t last )
{
    return need_reload(last);
}
