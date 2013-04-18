#ifndef _DEMO_APP_H_
#define _DEMO_APP_H_

//#include "timer_engine.h"
#include <string.h>
#include <log4cplus/logger.h>
#include <log4cplus/loggingmacros.h>
#include <log4cplus/configurator.h>
#include <log4cplus/consoleappender.h>
#include <log4cplus/layout.h>
#include <log4cplus/helpers/pointer.h>
#include <log4cplus/ndc.h>
#include <log4cplus/hierarchy.h>
#include "app_interface.h"

/*
void* app_initialize(void* container);
// 销毁
void app_finalize(void* app_inst);
// 处理tick

void app_handle_tick(void* app_inst);

// 返回buf中的package大小
int app_get_msgpack_size(const msgpack_context_t* ctx, const char* data, size_t size);

// 处理一个package
int app_handle_msgpack(void* app_inst, const msgpack_context_t* ctx, const char* data, size_t size);
*/
class demo_app_t
{
public:
    demo_app_t()
    {
        last_handle_reload_ = 0;
        logger_ = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("dempapp"));
    }

    ~demo_app_t() {}
    void handle_tick();
    int handle_msgpack(msgpack_context_t ctx, const char*, size_t);
    void set_container(void* c) { container_ = c; }

    void handle_timer(int64_t v);
    time_t last_handle_reload_;
    //timer_engine_t timers_;
    log4cplus::Logger logger_;
    void* container_;
};

#endif
