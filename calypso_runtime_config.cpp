#include "calypso_runtime_config.h"
#include "utility.h"
#include "log_interface.h"
#include <json/json.h>
#include <string>
using namespace std;

int calypso_runtime_config_t::load( const char* config_path )
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

    connect_timeout_ = conf_root.get("connect_timeout", 3).asInt();
    max_tcp_idle_ = conf_root.get("max_tcp_idle", 300).asInt();
    max_recover_link_num_ = conf_root.get("max_recover_link_num", 128).asInt();
    max_check_link_num_ = conf_root.get("max_check_link_num", 128).asInt();
    app_queue_len_ = conf_root.get("app_queue_len", 10240).asInt();
    deprecated_thread_life_ = conf_root.get("deprecated_thread_life", 120).asInt();
    deprecated_thread_idle_life_ = conf_root.get("deprecated_thread_idle_life", 5).asInt();

    C_INFO("connect_timeout=%d", connect_timeout_);
    C_INFO("max_tcp_idle=%d", max_tcp_idle_);
    C_INFO("max_recover_link_num=%d", max_recover_link_num_);
    C_INFO("max_check_link_num=%d", max_check_link_num_);
    C_INFO("app_queue_len=%d", app_queue_len_);
    C_INFO("deprecated_thread_life=%d", deprecated_thread_life_);
    C_INFO("deprecated_thread_idle_life=%d", deprecated_thread_idle_life_);
    return 0;
}
