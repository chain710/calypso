#ifndef _LOG_HARVESTER_H_
#define _LOG_HARVESTER_H_

#include <string>
#include <map>
#include <time.h>

/*
 *	监视日志变化，将新增日志发送到aggregator
 * TODO: 本地日志聚合
 */

class LogHarvester
{
public:
    LogHarvester();

    // create inotify_
    int create();

    // watch fd to watch_fds_
    int add_watch(const char* node_name, const char* file_pattern);

    // invoke eventhandle iff newline append to file
    int event_check();

    // check if real filename changed
    int watch_check();
private:
    std::string get_real_filename(const char* file_pattern, time_t t);
    void read_new_log(int fd);

    class Watcher
    {
    public:
        Watcher() { clear(); }
        void clear()
        {
            fd_ = -1;
            prev_offset_ = 0;
            filename_.clear();
            pattern_.clear();
            node_name_.clear();
        }

        int fd_;
        off_t prev_offset_;
        std::string filename_;
        std::string pattern_;
        std::string node_name_;
    };

    int inotify_;
    // fd -> watcher
    typedef std::map<int, Watcher> fd2watcher_map_t;
    fd2watcher_map_t watchers_;
};

#endif
