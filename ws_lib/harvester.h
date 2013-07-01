#ifndef _LOG_HARVESTER_H_
#define _LOG_HARVESTER_H_

#include <string>
#include <map>
#include <time.h>
#include <tr1/functional>
/*
 *	监视日志变化
 * TODO: 本地日志聚合
 */

class LogHarvester
{
public:
    // filename, newline
    typedef std::tr1::function<void (const std::string&, const std::string&)> newlog_callback_t;

    LogHarvester();

    // create inotify_
    int create();

    void clear_watchers();

    void remove_watcher(int wd);

    // watch fd to watch_fds_
    int add_watch(const std::string& fn);

    // invoke eventhandle iff newline append to file
    int event_check();

    void reg_newlog_callback(const newlog_callback_t& callback) { callback_ = callback; }
private:
    void read_new_log(int fd);
    // TODO: 太长时间没有notify的，关闭
    class Watcher
    {
    public:
        Watcher() { clear(); }
        Watcher(const Watcher& m)
        {
            fd_ = m.fd_;
            prev_offset_ = m.prev_offset_;
            filename_ = m.filename_;
        }

        void clear()
        {
            fd_ = -1;
            prev_offset_ = 0;
            filename_.clear();
        }

        int fd_;
        off_t prev_offset_;
        std::string filename_;
    };

    int inotify_;
    // fd -> watcher
    typedef std::map<int, Watcher> fd2watcher_map_t;
    fd2watcher_map_t watchers_;
    newlog_callback_t callback_;
};

#endif
