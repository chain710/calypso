#ifndef _DEMO_APP_H_
#define _DEMO_APP_H_

//#include "timer_engine.h"
#include <string.h>
#include "app_interface.h"
#include "demo_log.h"
/*
void* app_initialize(void* container);
// ����
void app_finalize(void* app_inst);
// ����tick

void app_handle_tick(void* app_inst);

// ����buf�е�package��С
int app_get_msgpack_size(const msgpack_context_t* ctx, const char* data, size_t size);

// ����һ��package
int app_handle_msgpack(void* app_inst, const msgpack_context_t* ctx, const char* data, size_t size);
*/
class demo_app_t
{
public:
    demo_app_t()
    {
        last_handle_reload_ = 0;
    }

    ~demo_app_t() {}
    void handle_tick();
    int handle_msgpack(msgpack_context_t ctx, const char*, size_t);
    void set_container(void* c) { container_ = c; }

    //void handle_timer(int64_t v);
    time_t last_handle_reload_;
    //timer_engine_t timers_;
    void* container_;


};

#endif
