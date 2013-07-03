#include "calypso_stat.h"
#include "utility.h"
#include <string.h>

#include <log4cplus/logger.h>
#include <log4cplus/loggingmacros.h>
#include <log4cplus/configurator.h>
#include <log4cplus/consoleappender.h>
#include <log4cplus/layout.h>
#include <log4cplus/helpers/pointer.h>
#include <log4cplus/ndc.h>
#include <log4cplus/hierarchy.h>

#define STAT_LOG_INST (log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("calypso-stat")))
#define STAT_LOG(logFmt, ...) LOG4CPLUS_INFO_FMT(STAT_LOG_INST, logFmt, __VA_ARGS__)

calypso_stat_t::calypso_stat_t()
{
    thread_num_ = 0;
    thread_stats_ = NULL;
}

calypso_stat_t::~calypso_stat_t()
{
    if (thread_stats_)
    {
        delete []thread_stats_;
        thread_stats_ = NULL;
        thread_num_ = 0;
    }
}

int calypso_stat_t::init( int thread_num )
{
    if (thread_num < 0)
    {
        return -1;
    }

    thread_num_ = thread_num;
    if (thread_num_ > 0)
    {
        thread_stats_ = new thread_stat_t[thread_num_];
        if (NULL == thread_stats_)
        {
            return -1;
        }
    }

    reset_stat();
    return 0;
}

void calypso_stat_t::reset_stat()
{
    if (thread_stats_)
    {
        memset(thread_stats_, 0, thread_num_*sizeof(thread_stat_t));
    }
    
    send_bytes_ = 0;
    recv_bytes_ = 0;
    send_packs_ = 0;
    recv_packs_ = 0;
    accept_conn_num_ = 0;
    closed_conn_num_ = 0;
    error_conn_num_ = 0;
}

void calypso_stat_t::incr_thread_send_bytes( int idx, int num )
{
    if (idx < 0 || idx >= thread_num_) return;
    thread_stats_[idx].send_bytes_ += num;
}

void calypso_stat_t::incr_thread_send_packs( int idx, int num )
{
    if (idx < 0 || idx >= thread_num_) return;
    thread_stats_[idx].send_packs_ += num;
}

void calypso_stat_t::incr_thread_recv_bytes( int idx, int num )
{
    if (idx < 0 || idx >= thread_num_) return;
    thread_stats_[idx].recv_bytes_ += num;
}

void calypso_stat_t::incr_thread_recv_packs( int idx, int num )
{
    if (idx < 0 || idx >= thread_num_) return;
    thread_stats_[idx].recv_packs_ += num;
}

void calypso_stat_t::set_thread_inq_free( int idx, int size )
{
    if (idx < 0 || idx >= thread_num_) return;
    thread_stats_[idx].in_queue_free_ = size;
}

void calypso_stat_t::set_thread_outq_free( int idx, int size )
{
    if (idx < 0 || idx >= thread_num_) return;
    thread_stats_[idx].out_queue_free_ = size;
}

void calypso_stat_t::write_and_clear()
{
    char time_str[128];
    format_time(time(NULL), "%Y-%m-%d %H:%M:%S", time_str, sizeof(time_str));
    STAT_LOG("==============stat at %s==============", time_str);
    STAT_LOG("[bytes] up:%d down:%d", recv_bytes_, send_bytes_);
    STAT_LOG("[packs] up:%d down:%d", recv_packs_, send_packs_);
    STAT_LOG("[conns] accept:%d closed:%d", accept_conn_num_, closed_conn_num_);
    for (int i = 0; i < thread_num_; ++i)
    {
        STAT_LOG("thread[%d]", i);
        STAT_LOG("\t[bytes] up:%d down:%d", thread_stats_[i].recv_bytes_, thread_stats_[i].send_bytes_);
        STAT_LOG("\t[packs] up:%d down:%d", thread_stats_[i].recv_packs_, thread_stats_[i].send_packs_);
        STAT_LOG("\t[queue] in-free:%d out-free:%d", thread_stats_[i].in_queue_free_, thread_stats_[i].out_queue_free_);
    }

    reset_stat();
}
