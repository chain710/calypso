#include "calypso_util.h"
#include "log_interface.h"
#include <fstream>
#include <time.h>

using namespace std;

// 为了让app线程也能获取信号信息，这里记录下上次获取各类信号的时间。同时各线程内需要记录处理时间
time_t reload_sig_time_ = 0;
time_t restart_app_sig_time_ = 0;
int stop_sig_ = 0;
int restart_sig_ = 0;

int read_all_text( const char* file_path, std::string& text )
{
    if (NULL == file_path)
    {
        C_FATAL("config_path is %p", file_path);
        return -1;
    }

    fstream fin(file_path, ios_base::in);
    if (!fin.is_open())
    {
        C_ERROR("netlink config %s not opened, maybe not exist?", file_path);
        return -1;
    }

    char buf[1024];
    while (!fin.eof() && !fin.fail())
    {
        fin.read(buf, sizeof(buf));
        text.append(buf, fin.gcount());
    }

    fin.close();
    return 0;
}

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

int nrand(int rbeg, int rend)
{
    // incorrect, but enough for now
    if (rend <= rbeg)
    {
        return rbeg;
    }

    return (rand() % (rend - rbeg + 1)) + rbeg;
}

void init_rand()
{
    srand(time(NULL));
}

bool need_restart_app()
{
    return restart_sig_ > 0;
}

void set_restart_app_sig()
{
    restart_sig_ = 1;
}

void clear_restart_app_sig()
{
    restart_sig_ = 0;
}
