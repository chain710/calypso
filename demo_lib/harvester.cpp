#include "harvester.h"
#include "demo_log.h"
#include <sys/inotify.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

using namespace std;

void StringReplace(const string& s, const string& oldsub,
                   const string& newsub, bool replace_all,
                   string* res) {
                       if (oldsub.empty()) {
                           res->append(s);  // if empty, append the given string.
                           return;
                       }

                       string::size_type start_pos = 0;
                       string::size_type pos;
                       do {
                           pos = s.find(oldsub, start_pos);
                           if (pos == string::npos) {
                               break;
                           }
                           res->append(s, start_pos, pos - start_pos);
                           res->append(newsub);
                           start_pos = pos + oldsub.size();  // start searching again after the "old"
                       } while (replace_all);
                       res->append(s, start_pos, s.length() - start_pos);
}

// ----------------------------------------------------------------------
// StringReplace()
//    Give me a string and two patterns "old" and "new", and I replace
//    the first instance of "old" in the string with "new", if it
//    exists.  If "global" is true; call this repeatedly until it
//    fails.  RETURN a new string, regardless of whether the replacement
//    happened or not.
// ----------------------------------------------------------------------

string StringReplace(const string& s, const string& oldsub,
                     const string& newsub, bool replace_all) {
                         string ret;
                         StringReplace(s, oldsub, newsub, replace_all, &ret);
                         return ret;
}

int SetNonBlock(int fd)
{
    int flags;

    if ((flags = fcntl(fd, F_GETFL, 0)) < 0)
    {
        return -1;
    }

    flags |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags) < 0)
    {
        return -1;
    }

    return 0;
}

LogHarvester::LogHarvester()
{
    inotify_ = -1;
}

int LogHarvester::create()
{
    inotify_ = inotify_init();
    if (inotify_ < 0)
    {
        L_ERROR("inotify_init error %d", errno);
        return -1;
    }

    SetNonBlock(inotify_);
    return 0;
}

int LogHarvester::add_watch( const char* node_name, const char* file_pattern )
{
    string fn = get_real_filename(file_pattern, time(NULL));
    int fd = inotify_add_watch(inotify_, fn.c_str(), IN_MODIFY|IN_DELETE_SELF|IN_MOVE_SELF);
    if (fd < 0)
    {
        L_ERROR("add watch %s error %d", fn.c_str(), errno);
        return -1;
    }

    Watcher& w = watchers_[fd];
    w.fd_ = fd;
    w.filename_ = fn;
    w.pattern_ = file_pattern;
    w.node_name_ = node_name;

    return 0;
}

int LogHarvester::event_check()
{
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(inotify_, &rfds);
    timeval timeout = {0, 0};
    int ret = select(inotify_+1, &rfds, NULL, NULL, &timeout);
    if (ret > 0)
    {
        char rd_buf[1024];
        int r;
        int rdoffset = 0, poffset, peventlen;
        inotify_event *pevent;

        while (true)
        {
            r = read(inotify_, &rd_buf[rdoffset], sizeof(rd_buf)-rdoffset);
            if (r <= 0)
            {
                if (r < 0 && errno != EAGAIN)
                {
                    L_ERROR("ready inotify fd error %d", errno);
                }

                break;
            }

            poffset = 0;
            while (poffset < r)
            {
                pevent = (inotify_event *)&rd_buf[poffset];
                peventlen = pevent->len + sizeof(inotify_event);
                if (r - poffset >= peventlen)
                {
                    //read newline from this file
                    //if not newline, skip
                    //event handler(node_name, contentline)
                    if (pevent->mask & IN_MODIFY)
                    {
                        // read last line
                        L_INFO("write event from fd %d", pevent->wd);
                        read_new_log(pevent->wd);
                    }

                    if (pevent->mask & IN_DELETE_SELF)
                    {
                        L_INFO("delete event from fd %d", pevent->wd);
                        close(pevent->wd);
                        watchers_.erase(pevent->wd);
                    }

                    if (pevent->mask & IN_MOVE_SELF)
                    {
                        L_INFO("file is moved, fd %d", pevent->wd);
                        // TODO:close and addnew
                    }

                    poffset += peventlen;
                }
                else
                {
                    memmove(rd_buf, &rd_buf[poffset], r-poffset);
                    rdoffset = r-poffset;
                    break;
                }
            }
        }
    }
    else if (ret < 0)
    {
        L_ERROR("select inotify failed, errno %d", errno);
        return -1;
    }

    return 0;
}

std::string LogHarvester::get_real_filename( const char* file_pattern, time_t t )
{
    string fn(file_pattern);
    char tmp[16];
    struct tm *filetm;
    filetm = localtime(&t);
    if (NULL == filetm)
    {
        return fn;
    }

    strftime(tmp, sizeof(tmp), "%Y", filetm);
    StringReplace(fn, "{YEAR}", tmp, true);
    strftime(tmp, sizeof(tmp), "%m", filetm);
    StringReplace(fn, "{MONTH}", tmp, true);
    strftime(tmp, sizeof(tmp), "%d", filetm);
    StringReplace(fn, "{DAY}", tmp, true);
    strftime(tmp, sizeof(tmp), "%H", filetm);
    StringReplace(fn, "{HOUR}", tmp, true);
    strftime(tmp, sizeof(tmp), "%M", filetm);
    StringReplace(fn, "{MIN}", tmp, true);

    return fn;
}

int LogHarvester::watch_check()
{
    // check if all filename correct
    int change_num = 0;
    string fn;
    time_t now = time(NULL);
    for (fd2watcher_map_t::iterator it = watchers_.begin(); it != watchers_.end(); )
    {
        fn = get_real_filename(it->second.pattern_.c_str(), now);
        if (fn != it->second.filename_)
        {
            // switch to new file
            add_watch(it->second.node_name_.c_str(), it->second.pattern_.c_str());
            close(it->second.fd_);

            watchers_.erase(it++);
            ++change_num;
        }
        else
        {
            ++it;
        }
    }

    return change_num;
}

void LogHarvester::read_new_log( int fd )
{
    fd2watcher_map_t::iterator it = watchers_.find(fd);
    if (it == watchers_.end())
    {
        return;
    }

    Watcher& w = it->second;
    int rdfd = open(w.filename_.c_str(), O_RDONLY);
    if (rdfd < 0)
    {
        L_ERROR("open %s errno %d", w.filename_.c_str(), errno);
        return;
    }

    SetNonBlock(rdfd);
    struct stat file_stat;
    int err = fstat(rdfd, &file_stat);
    if (err < 0)
    {
        L_ERROR("fstat %d error %d", rdfd, errno);
        return;
    }

    if (file_stat.st_size < w.prev_offset_ || 0 == w.prev_offset_)
    {
        // file may be truncated
        w.prev_offset_ = file_stat.st_size;
        return;
    }

    string linebuf;
    char rdbuf[1024];
    int r;
    size_t delim_pos;
    err = lseek(rdfd, w.prev_offset_, SEEK_SET);
    while (true)
    {
        r = read(rdfd, rdbuf, sizeof(rdbuf));
        if (r <= 0)
        {
            if (0 == r || errno == EAGAIN) break;
            if (errno == EINTR) continue;
        }

        linebuf.append(rdbuf, r);
    }

    close(rdfd);

    int linebuf_off = 0;
    int log_len;
    do 
    {
        delim_pos = linebuf.find("\n", linebuf_off);
        if (delim_pos == string::npos) break;
        log_len = delim_pos-linebuf_off;
        L_INFO("new log: %s", linebuf.substr(linebuf_off, log_len).c_str());
        linebuf.erase(linebuf_off, log_len+1);
        linebuf_off += log_len+1;
    } while (string::npos != delim_pos);

    w.prev_offset_ += linebuf_off;

    // contents remained in linebuf discarded!!!!!
}

