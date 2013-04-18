#include "calypso_bootstrap_config.h"
#include "utility.h"
#include "log_interface.h"
#include <json/json.h>
using namespace std;

int calypso_bootstrap_config_t::load( const char* config_path )
{
    string json_raw;
    if (read_all_text(config_path, json_raw))
    {
        return -1;
    }

    Json::Value conf_root;
    Json::Reader reader;
    bool succ = reader.parse(json_raw, conf_root);
    if (!succ)
    {
        C_ERROR("parse config(%s) failed, %s", config_path, reader.getFormattedErrorMessages().c_str());
        return -1;
    }

    netlink_config_path_ = conf_root.get("netlink_config", "netlink.json").asString();
    max_link_num_ = conf_root.get("max_link_num", 1024).asInt();
    max_fired_link_num_ = conf_root.get("max_fired_link_num", 1024).asInt();
    runtime_config_path_ = conf_root.get("runtime_config", "runtime.json").asString();

    Json::Value& allocator_array = conf_root["mem_allocator"];
    if (!allocator_array.isArray())
    {
        C_ERROR("expect %s to be ARRAY type!", "mem_allocator");
        return -1;
    }

    allocator_config_.clear();
    int array_size = allocator_array.size();
    allocator_tuple_t tmp_tuple;
    for (int i = 0; i < array_size; ++i)
    {
        Json::Value& config_item = allocator_array[i];
        if (!config_item.isArray() || config_item.size() != 2)
        {
            C_ERROR("invalid allocator config item, plz check %s section", "mem_allocator");
            return -1;
        }

        tmp_tuple.size_ = config_item[0].asInt();
        tmp_tuple.capacity_ = config_item[1].asInt();
        allocator_config_.push_back(tmp_tuple);
    }

    C_INFO("netlink_config=%s", netlink_config_path_.c_str());
    C_INFO("max_link_num=%d", max_link_num_);
    C_INFO("max_fired_link_num=%d", max_fired_link_num_);
    C_INFO("mem_allocator:%s", "");
    for (int i = 0; i < (int)allocator_config_.size(); ++i)
    {
        C_INFO("\t[%d]cap=%d size=%d", i, allocator_config_[i].capacity_, allocator_config_[i].size_);
    }

    return 0;
}
