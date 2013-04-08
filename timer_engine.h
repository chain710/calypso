#ifndef _TIMER_ENGINE_H_
#define _TIMER_ENGINE_H_

#include <tr1/functional>
#include <stdint.h>
#include <sys/time.h>
#include "rbtree.h"

class timer_engine_t
{
public:
    enum timer_flag_t
    {
        permenant_timer = 0x00000001,
    };

    struct timer_t
    {
        // 定时结束时间
        timeval deadline_;
        // 定时间隔, 单位：millisecond(如果是循环定时器需要deadline+interval产生下一次的deadline)
        int interval_;
        // 用户数据
        int64_t user_;
        // 标志位
        unsigned int flag_;
    };

    typedef std::tr1::function<void (int64_t)> timer_callback;

    timer_engine_t();
    virtual ~timer_engine_t();
    int init(int capacity);
    int add_timer(timer_t t);
    int walk(timer_callback callback);
private:
    // 红黑树组织所有定时器资源，按deadline排序。遍历时getmin直到deadline未达到
    struct timer_key_t
    {
        timeval deadline_;
        int idx_;
    };

    struct _time_cmp
    {
        int operator()(const timer_key_t& l, const timer_key_t& r)
        {
            if (l.deadline_.tv_sec == r.deadline_.tv_sec
                && l.deadline_.tv_usec == r.deadline_.tv_usec)
            {
                return l.idx_ - r.idx_;
            }

            return 1000000 * (l.deadline_.tv_sec - r.deadline_.tv_sec) - (l.deadline_.tv_usec - r.deadline_.tv_usec);
        }
    };

    // deny copy-cons
    timer_engine_t(const timer_engine_t&) {}

    llrbtree_t<timer_key_t, timer_t, _time_cmp> timer_tree_;
    int global_idx_;
};

#endif

