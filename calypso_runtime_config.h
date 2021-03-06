#ifndef _CALYPSO_RUNTIME_CONFIG_H_
#define _CALYPSO_RUNTIME_CONFIG_H_

class calypso_runtime_config_t
{
public:
    int load(const char* config_path);
    int get_connect_timeout() const { return connect_timeout_; }
    int get_max_tcp_idle() const { return max_tcp_idle_; }
    int get_max_recover_link_num() const { return max_recover_link_num_; }
    int get_max_check_link_num() const { return max_check_link_num_; }
    int get_app_queue_len() const { return app_queue_len_; }
    int get_deprecated_thread_life() const { return 0; }
    int get_deprecated_thread_idle_life() const { return 0; }
    int get_min_recover_link_interval() const { return min_netlink_recover_interval_; }
    int get_stat_interval() const { return stat_interval_; }
private:
    int connect_timeout_;
    int max_tcp_idle_;
    int max_recover_link_num_;
    int max_check_link_num_;
    int app_queue_len_;
    int deprecated_thread_life_;
    int deprecated_thread_idle_life_;
    int min_netlink_recover_interval_;
    int stat_interval_;
};

#endif
