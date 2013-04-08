#include "timer_engine.h"

using namespace std;
using namespace std::tr1;

timer_engine_t::timer_engine_t()
{
    global_idx_ = 0;
}

timer_engine_t::~timer_engine_t()
{
}

int timer_engine_t::add_timer( timer_t t )
{
    timer_key_t key;
    memset(&key, 0, sizeof(key));
    key.idx_ = global_idx_;
    key.deadline_ = t.deadline_;
    int ret = timer_tree_.insert(key, t);
    if (ret < 0)
    {
        return -1;
    }

    ++global_idx_;
    return 0;
}

int timer_engine_t::walk( timer_callback callback )
{
    int walk_num = 0;
    timeval nowtime;
    gettimeofday(&nowtime, NULL);
    timer_engine_t::timer_key_t *key;
    timer_engine_t::timer_t val;
    int ret;
    while (true)
    {
        ++walk_num;
        key = timer_tree_.get_min_key();
        if (NULL == key)
        {
            break;
        }

        if (timercmp(&nowtime, &key->deadline_, <))
        {
            break;
        }

        val = *timer_tree_.get_by_key(*key);
        callback(val.user_);

        // É¾³ı¸Ãkey
        ret = timer_tree_.remove(*key);
        if (ret < 0)
        {
            // FATAL
            break;
        }

        if (val.flag_ & permenant_timer)
        {
            // Èû»ØÈ¥
            val.deadline_ = nowtime;
            val.deadline_.tv_usec += 1000*val.interval_;
            val.deadline_.tv_sec += val.deadline_.tv_usec / 1000000;
            val.deadline_.tv_usec %= 1000000;
            ret = add_timer(val);
            if (ret < 0)
            {
                // FATAL
                break;
            }
        }
    }

    return walk_num;
}

int timer_engine_t::init( int capacity )
{
    return timer_tree_.initialize(capacity);
}
