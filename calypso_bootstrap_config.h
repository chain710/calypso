#ifndef _CALYPSO_BOOTSTRAP_CONFIG_H_
#define _CALYPSO_BOOTSTRAP_CONFIG_H_

/*
{
    // global allocator [[size, capacity], ...]
    "mem_allocator":[[512, 1000], [1024, 1000]], 
    // network config path
    "netlink_config": , 
    // max fd num
    "max_link_num":,
    // max fired fd num once
    "max_fired_link_num":,
}
 */

#include <vector>
#include <string>

class calypso_bootstrap_config_t
{
public:

    struct allocator_tuple_t
    {
        int size_;
        int capacity_;
    };

    typedef std::vector<allocator_tuple_t> alloc_conf_list_t;

    int load(const char* config_path);

    const char* get_netlink_config_path() const { return netlink_config_path_.c_str(); }
    const char* get_runtime_config_path() const { return runtime_config_path_.c_str(); }
    const alloc_conf_list_t& get_allocator_config() const { return allocator_config_; }
    int get_max_link_num() const { return max_link_num_; }
    int get_max_fired_link_num() const { return max_fired_link_num_; }
    int get_thread_num() const { return thread_num_; }
    const char* get_bootstrap_config_path() const { return bootstrap_config_path_.c_str(); }
private:
    std::string bootstrap_config_path_;
    std::string netlink_config_path_;
    std::string runtime_config_path_;
    alloc_conf_list_t allocator_config_;

    int max_link_num_;
    int max_fired_link_num_;
    int thread_num_;
};

#endif
