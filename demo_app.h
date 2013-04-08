#ifndef _DEMO_APP_H_
#define _DEMO_APP_H_

#include "app_handler.h"
#include <string.h>

class demo_app_t: public app_handler_t
{
public:
    demo_app_t()
    {
        main_inst_ = NULL;
        last_handle_reload_ = 0;
    }

    ~demo_app_t() {}
    virtual void handle_tick();
    virtual int get_msgpack_size(msgpack_context_t ctx, const char*, size_t) const;
    virtual int handle_msgpack(msgpack_context_t ctx, const char*, size_t);

    void* main_inst_;
    time_t last_handle_reload_;
};

#endif
