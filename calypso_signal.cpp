#include "calypso_signal.h"

// 为了让app线程也能获取信号信息，这里记录下上次获取各类信号的时间。同时各线程内需要记录处理时间
time_t reload_sig_time_ = 0;
time_t restart_app_sig_time_ = 0;
int stop_sig_ = 0;
int restart_sig_ = 0;

bool need_reload( time_t last_time )
{
    return reload_sig_time_ > last_time;
}

bool need_stop()
{
    return stop_sig_ > 0;
}

void set_reload_time()
{
    reload_sig_time_ = time(NULL);
}

void set_stop_sig()
{
    stop_sig_ = 1;
}

void clear_reload_time()
{
    reload_sig_time_ = 0;
}

void clear_stop_sig()
{
    stop_sig_ = 0;
}


