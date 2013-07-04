#ifndef _NETLINK_CONFIG_H_
#define _NETLINK_CONFIG_H_

#include <json/json.h>
#include <string>
#include <tr1/functional>
#include <map>
#include <vector>

class netlink_config_t
{
public:
    struct config_item_t
    {
        char type_[12];
        char ip_[16];
        unsigned short port_;
        char bind_ip_[16];
        unsigned short bind_port_;
        int sys_send_buffer_;
        int sys_recv_buffer_;
        int usr_send_buffer_;
        int usr_recv_buffer_;
        int back_log_;
        unsigned int mask_; // control if event happened on this link is allowed to be transfered to appthread
        unsigned reuse_addr_:1;
        unsigned keep_alive_:1;
    };

    typedef std::tr1::function<int (int idx, const config_item_t&, void*)> walk_link_callback;

    int load(const char* config_path);
    // 遍历链路配置，同时重建group2idx
    void walk(walk_link_callback callback, void* up);
    // 遍历链路配置(reload)，同时重建group2idx
    void walk_diff(const netlink_config_t& old, 
        const walk_link_callback& close_callback, 
        const walk_link_callback& open_callback, 
        const walk_link_callback& update_callback, 
        void* up);
    std::vector<int> get_linkid_by_group(int group) const;
private:
    int load_text(const char* config_path, std::string& text);
    void make_link_sig(const config_item_t& item, std::string& out);
    void make_config_item(const Json::Value& item, config_item_t& out);

    typedef std::map<int, std::vector<int> > group2idx_t;
    Json::Value conf_;
    // group -> lindid
    group2idx_t group2idx_;
};

#endif
