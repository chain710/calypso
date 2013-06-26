#include "demo_app.h"
#include <string.h>
#include <stdio.h>
#include <string>
#include <signal.h>

using namespace std;
using namespace log4cplus;

int demo_app_t::handle_msgpack( msgpack_context_t ctx, const char* pack, size_t pack_len)
{
    if (ctx.flag_ & mpf_new_connection)
    {
        char hello_msg[128];
        snprintf(hello_msg, sizeof(hello_msg), "welcome to rapture!\r\n");
        return calypso_send_msgpack_by_ctx(container_, &ctx, hello_msg, strlen(hello_msg));
    }
    else
    {
        string msg;
        msg.append(pack, pack_len);
        LOG4CPLUS_DEBUG_FMT(logger_, "recv client msg(%s)\n", msg.c_str());
        if (0 == msg.find("END"))
        {
            ctx.flag_ |= mpf_close_link;
        }

        return calypso_send_msgpack_by_ctx(container_, &ctx, pack, pack_len);
    }
}

void demo_app_t::handle_tick()
{
    if (calypso_need_reload(last_handle_reload_))
    {
        // process reload
        LOG4CPLUS_DEBUG_FMT(logger_, "recv reload sig %u", (unsigned int)last_handle_reload_);
        last_handle_reload_ = time(NULL);
    }

    //timer_engine_t::timer_callback on_timer_func = std::tr1::bind(&demo_app_t::handle_timer, this, tr1::placeholders::_1);
    //timers_.walk(on_timer_func);
}

void* app_initialize( void* container )
{
    demo_app_t* r = new demo_app_t;
    r->set_container(container);
    return r;
}

void app_finalize( void* app_inst )
{
    demo_app_t* app = (demo_app_t*) app_inst;
    delete app;
}

void app_handle_tick( void* app_inst )
{
    demo_app_t* app = (demo_app_t*) app_inst;
    app->handle_tick();
}

int app_get_msgpack_size( const msgpack_context_t* ctx, const char* data, size_t size )
{
    std::string tmp;
    tmp.assign(data, size);
    std::string::size_type pos = tmp.find("\r\n");
    if (pos == std::string::npos)
    {
        return 0;
    }

    return pos+2;
}

int app_handle_msgpack( void* app_inst, const msgpack_context_t* ctx, const char* data, size_t size )
{
    demo_app_t* app = (demo_app_t*) app_inst;
    return app->handle_msgpack(*ctx, data, size);
}

app_handler_t get_app_handler()
{
    app_handler_t h;
    memset(&h, 0, sizeof(h));
    h.init_ = app_initialize;
    h.fina_ = app_finalize;
    h.get_msgpack_size_ = app_get_msgpack_size;
    h.handle_msgpack_ = app_handle_msgpack;
    h.handle_tick_ = app_handle_tick;
    return h;
}
