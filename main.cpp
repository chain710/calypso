#include "log_interface.h"
#include "calypso.h"
#include "calypso_signal.h"
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <google/gflags.h>

using namespace google;
using namespace log4cplus;

void sig_handler(int sig)
{
    switch (sig)
    {
    case SIGHUP:
        set_reload_time();
        break;
    case SIGINT:
    case SIGQUIT:
    case SIGTERM:
        set_stop_sig();
        break;
    default:
        C_WARN("unsupported signal %d", sig);
        break;
    }
}

//////////////////////////////////////////////////////////////////////////
// program utility
//////////////////////////////////////////////////////////////////////////

int lock_prog(const char* pid_file)
{
    if (NULL == pid_file)
    {
        fprintf(stderr, "pid_file is NULL\n");
        return -1;
    }

    int fd = open(pid_file, O_RDONLY);
    if (fd < 0)
    {
        fprintf(stderr, "open %s failed errno %d\n", pid_file, errno);
        return -1;
    }

    if (flock(fd, LOCK_EX|LOCK_NB) < 0)
    {
        if (errno == EWOULDBLOCK)
        {
            fprintf(stderr, "pidfile %s already locked, another instance is running?", pid_file);
            return -1;
        }

        fprintf(stderr, "flock failed errno %d\n", errno);
        return -1;
    }

    return fd;
}

void save_pid(const pid_t pid, const char *pid_file) {
    FILE *fp;
    if (pid_file == NULL)
        return;

    if ((fp = fopen(pid_file, "w")) == NULL) {
        fprintf(stderr, "Could not open the pid file %s for writing\n", pid_file);
        return;
    }

    fprintf(fp,"%ld\n", (long)pid);
    if (fclose(fp) == -1) {
        fprintf(stderr, "Could not close the pid file %s.\n", pid_file);
        return;
    }
}

void remove_pidfile(const char *pid_file) {
    if (pid_file == NULL)
        return;

    if (unlink(pid_file) != 0) {
        fprintf(stderr, "Could not remove the pid file %s.\n", pid_file);
    }
}

//////////////////////////////////////////////////////////////////////////
// program entry
//////////////////////////////////////////////////////////////////////////

DEFINE_bool(daemon, false, "run as daemon");
DEFINE_string(pidfile, "", "specify pid file");
DEFINE_string(conf, "app.json", "specify app launch config file");

const char* USAGE_MSG = "calypso";
const char* PROG_VERSION = "1.0.0";
const int PROG_REVISION = 1;

int main(int argc, char** argv)
{
    int ret;
    int lock_fd = -1;

    /* set stderr non-buffering */
    setbuf(stderr, NULL);

    SetUsageMessage(USAGE_MSG);
    SetVersionString(PROG_VERSION);
    ParseCommandLineFlags(&argc, &argv, true);

    if (FLAGS_daemon)
    {
        ret = daemon(1, 1);
        if (ret < 0)
        {
            fprintf(stderr, "failed to daemon() in order to daemonize\n");
            return -1;
        }
    }

    if (!FLAGS_pidfile.empty())
    {
        save_pid(getpid(), FLAGS_pidfile.c_str());
        lock_fd = lock_prog(FLAGS_pidfile.c_str());
        if (lock_fd < 0)
        {
            return -1;
        }
    }

    // setup signal handler
    clear_reload_time();
    clear_stop_sig();
    if (SIG_ERR == signal(SIGHUP, sig_handler))
    {
        fprintf(stderr, "can not catch SIGHUP\n");
        return -1;
    }

    if (SIG_ERR == signal(SIGQUIT, sig_handler))
    {
        fprintf(stderr, "can not catch SIGQUIT\n");
        return -1;
    }

    if (SIG_ERR == signal(SIGTERM, sig_handler))
    {
        fprintf(stderr, "can not catch SIGTERM\n");
        return -1;
    }

    if (SIG_ERR == signal(SIGINT, sig_handler))
    {
        fprintf(stderr, "can not catch SIGINT\n");
        return -1;
    }

    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sa.sa_flags = 0;
    if (sigemptyset(&sa.sa_mask) == -1 || sigaction(SIGPIPE, &sa, 0) == -1) 
    {
        fprintf(stderr, "failed to ignore SIGPIPE");
        return -1;
    }

    // temporary basic config
    BasicConfigurator::doConfigure();

    calypso_main_t runner;
    ret = runner.initialize(FLAGS_conf.c_str());
    if (ret < 0)
    {
        fprintf(stderr, "calypso_main_t initialize failed %d\n", ret);
        return -1;
    }

    runner.reg_app_handler(get_app_handler());
    fprintf(stderr, "app launched successfully!\n");
    runner.run();

    if (!FLAGS_pidfile.empty())
    {
        // remove pid
        remove_pidfile(FLAGS_pidfile.c_str());
        if (lock_fd >= 0) close(lock_fd);
    }

    fprintf(stderr, "goodbye!\n");
    return 0;
}
