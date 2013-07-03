#ifndef _CALYPSO_STAT_H_
#define _CALYPSO_STAT_H_

class calypso_stat_t
{
public:
    calypso_stat_t();
    ~calypso_stat_t();
    int init(int thread_num);
    void reset_stat();

    void incr_send_bytes(int num) { send_bytes_ += num; }
    void incr_send_packs(int num) { send_packs_ += num; }
    void incr_recv_bytes(int num) { recv_bytes_ += num; }
    void incr_recv_packs(int num) { recv_packs_ += num; }
    void incr_accept_conn(int num) { accept_conn_num_ += num; }
    void incr_closed_conn(int num) { closed_conn_num_ += num; }
    void incr_error_conn(int num) { error_conn_num_ += num; }

    void incr_thread_send_bytes(int idx, int num);
    void incr_thread_send_packs(int idx, int num);
    void incr_thread_recv_bytes(int idx, int num);
    void incr_thread_recv_packs(int idx, int num);
    void set_thread_inq_free(int idx, int size);
    void set_thread_outq_free(int idx, int size);

    void write_and_clear();
private:
    struct thread_stat_t
    {
        int send_bytes_;
        int recv_bytes_;
        int send_packs_;
        int recv_packs_;
        int in_queue_free_;
        int out_queue_free_;
    };

    int thread_num_;
    thread_stat_t* thread_stats_;
    int send_bytes_;
    int recv_bytes_;
    int send_packs_;
    int recv_packs_;
    int accept_conn_num_;
    int closed_conn_num_;
    int error_conn_num_;
};

#endif
