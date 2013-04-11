#include "demo_app.h"
#include "log_interface.h"
#include "calypso_interface.h"
#include "calypso.h"    // main
#include "calypso_signal.h" // main
#include <string.h>
#include <stdio.h>
#include <log4cplus/configurator.h>
#include <string>
#include <signal.h>

using namespace std;
using namespace log4cplus;

int demo_app_t::get_msgpack_size( msgpack_context_t ctx, const char* data, size_t data_len) const
{
    return strnlen(data, data_len);
}

int demo_app_t::handle_msgpack( msgpack_context_t ctx, const char* pack, size_t pack_len)
{
    if (ctx.flag_ & mpf_new_connection)
    {
        char hello_msg[128];
        snprintf(hello_msg, sizeof(hello_msg), "welcome to rapture!\r\n");
        return calypso_send_msgpack_by_ctx(main_inst_, ctx, hello_msg, strlen(hello_msg));
    }
    else
    {
        string msg;
        msg.append(pack, pack_len);
        C_INFO("recv client msg(%s)\n", msg.c_str());
        return calypso_send_msgpack_by_ctx(main_inst_, ctx, pack, pack_len);
    }
}

void demo_app_t::handle_tick()
{
    if (need_reload(last_handle_reload_))
    {
        // process reload
        C_INFO("recv reload sig %u", (unsigned int)last_handle_reload_);
        last_handle_reload_ = time(NULL);
    }

    //timer_engine_t::timer_callback on_timer_func = std::tr1::bind(&demo_app_t::handle_timer, this, tr1::placeholders::_1);
    //timers_.walk(on_timer_func);
}

void sig_handler(int sig)
{
    switch (sig)
    {
    case SIGHUP:
        set_reload_time();
        break;
    case SIGQUIT:
        set_stop_sig();
        break;
    case SIGTERM:
        // restart app thread
        set_restart_app_sig();
        break;
    default:
        C_WARN("unsupported signal %d", sig);
        break;
    }
}

int main(int argc, char** argv)
{
    int verbose = 2;
    if (argc > 1)
    {
        verbose = strtol(argv[1], NULL, 10);
    }

    clear_reload_time();
    clear_stop_sig();
    clear_restart_app_sig();
    signal(SIGHUP, sig_handler);
    signal(SIGQUIT, sig_handler);
    signal(SIGTERM, sig_handler);

    switch (verbose)
    {
    case 0:
        C_LOG_INST.setLogLevel(ERROR_LOG_LEVEL);
        break;
    case 1:
        C_LOG_INST.setLogLevel(INFO_LOG_LEVEL);
        break;
    case 2:
        C_LOG_INST.setLogLevel(TRACE_LOG_LEVEL);
        break;
    }

    SharedAppenderPtr myAppender(new ConsoleAppender());
    myAppender->setName("a1");
    C_LOG_INST.addAppender(myAppender);
    std::auto_ptr<Layout> myLayout = std::auto_ptr<Layout>(new log4cplus::PatternLayout(LOG4CPLUS_TEXT("[%-5p][%x][%l] %m%n")));
    myAppender->setLayout(myLayout);

    calypso_main_t runner;
    runner.initialize("demo.json");

    demo_app_t app;
    app.main_inst_ = &runner;

    runner.register_handler(&app);
    runner.run();

    return 0;
}
