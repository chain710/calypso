#include "netlink_config.h"
#include "log_interface.h"
#include "utility.h"
#include <fstream>
#include <string>
#include <map>
#include <vector>
#include <string.h>
#include <stdio.h>

using namespace std;

int netlink_config_t::load( const char* config_path )
{
    string json_raw;
    if (load_text(config_path, json_raw))
    {
        return -1;
    }

    conf_.clear();
    Json::Reader reader;
    bool succ = reader.parse(json_raw, conf_);
    if (!succ)
    {
        C_ERROR("parse config failed, %s", reader.getFormattedErrorMessages().c_str());
        return -1;
    }

    if (!conf_["links"].isArray())
    {
        C_FATAL("expect \"%s\" to be ARRAY type", "links");
        return -1;
    }

    return 0;
}

int netlink_config_t::load_text( const char* config_path, std::string& text )
{
    if (NULL == config_path)
    {
        C_FATAL("config_path is %p", config_path);
        return -1;
    }

    fstream fin(config_path, ios_base::in);
    if (!fin.is_open())
    {
        C_ERROR("netlink config %s not opened, maybe not exist?", config_path);
        return -1;
    }

    char buf[1024];
    while (!fin.eof() && !fin.fail())
    {
        fin.read(buf, sizeof(buf));
        text.append(buf, fin.gcount());
    }

    fin.close();
    return 0;
}

void netlink_config_t::walk( walk_link_callback callback, void* up )
{
    group2idx_.clear();
    int link_size = conf_["links"].size();
    config_item_t config_item;
    int group, link_id;
    for (int i = 0; i < link_size; ++i)
    {
        Json::Value& cursor = conf_["links"][i];
        make_config_item(cursor, config_item);
        link_id = callback(cursor.get("_link_id", -1).asInt(), config_item, up);
        cursor["_link_id"] = link_id;
        group = cursor.get("group", -1).asInt();
        if (group >= 0 && link_id >= 1)
        {
            group2idx_[group].push_back(link_id);
        }
    }
}

void netlink_config_t::walk_diff( const netlink_config_t& old, 
                                 const walk_link_callback& close_callback, 
                                 const walk_link_callback& open_callback, 
                                 const walk_link_callback& update_callback, 
                                 void* up )
{
    group2idx_.clear();
    Json::Value& newlinks = conf_["links"];
    string link_sig;
    typedef map<string, vector<int> > sig2ids_map_t;
    sig2ids_map_t sig2id;
    config_item_t config_item;
    for (int i = 0; i < (int)newlinks.size(); ++i)
    {
        // linksig
        make_config_item(newlinks[i], config_item);
        make_link_sig(config_item, link_sig);
        sig2id[link_sig].push_back(i);
    }

    // close那些已经不在的
    const Json::Value& oldlinks = old.conf_["links"];
    for (int i = 0; i < (int)oldlinks.size(); ++i)
    {
        make_config_item(oldlinks[i], config_item);
        make_link_sig(config_item, link_sig);
        // find in new config? update _link_id, erase map
        sig2ids_map_t::iterator it = sig2id.find(link_sig);
        if (it != sig2id.end())
        {
            newlinks[it->second.back()]["_link_id"] = oldlinks[i].get("_link_id", -1).asInt();
            it->second.pop_back();
            if (it->second.empty())
            {
                sig2id.erase(link_sig);
            }
        }
        else
        {
            close_callback(oldlinks[i].get("_link_id", -1).asInt(), config_item, up);
        }
    }

    // 遍历剩下新增和没变化的
    int link_id, group;
    for (int i = 0; i < (int)newlinks.size(); ++i)
    {
        make_config_item(newlinks[i], config_item);
        link_id = newlinks[i].get("_link_id", -1).asInt();
        if (link_id < 0)
        {
            link_id = open_callback(-1, config_item, up);
        }
        else
        {
            // 以前就有的，更新option
            link_id = update_callback(link_id, config_item, up);
        }

        newlinks[i]["_link_id"] = link_id;
        group = newlinks[i].get("group", -1).asInt();
        if (group >= 0 && link_id >= 0)
        {
            group2idx_[group].push_back(link_id);
        }
    }
}

void netlink_config_t::make_link_sig( const config_item_t& item, std::string& out )
{
    char tmp[128];
    if (0 == strncmp(item.type_, "listen", sizeof(item.type_)))
    {
        snprintf(tmp, sizeof(tmp), "%s_%s:%d", item.type_, item.bind_ip_, item.bind_port_);
    }
    else
    {
        snprintf(tmp, sizeof(tmp), "%s_%s:%d_%s:%d", item.type_, 
            item.ip_, 
            item.port_, 
            item.bind_ip_, 
            item.bind_port_);
    }

    out = tmp;
}

void netlink_config_t::make_config_item( const Json::Value& item, config_item_t& out )
{
    snprintf(out.type_, sizeof(out.type_), "%s", item.get("type", "connect").asCString());
    snprintf(out.ip_, sizeof(out.ip_), "%s", item.get("ip", "0.0.0.0").asCString());
    out.port_ = item.get("port", 0).asInt();
    snprintf(out.bind_ip_, sizeof(out.bind_ip_), "%s", item.get("bind_ip", "").asCString());
    out.bind_port_ = item.get("bind_port", 0).asInt();
    out.sys_send_buffer_ = item.get("sys_send_buffer", 0).asInt();
    out.sys_recv_buffer_ = item.get("sys_recv_buffer", 0).asInt();
    out.usr_send_buffer_ = item.get("usr_send_buffer", 0).asInt();
    out.usr_recv_buffer_ = item.get("usr_recv_buffer", 0).asInt();
    out.back_log_ = item.get("back_log", 128).asInt();
    out.keep_alive_ = item.get("keep_alive", true).asBool()? 1: 0;
    out.reuse_addr_ = item.get("reuse_addr", true).asBool()? 1: 0;
    out.mask_ = item.get("mask", 0xFFFFFFFF).asUInt();
}

std::vector<int> netlink_config_t::get_linkid_by_group( int group ) const
{
    vector<int> linkids;
    group2idx_t::const_iterator it = group2idx_.find(group);
    if (it == group2idx_.end())
    {
        return linkids;
    }

    return it->second;
}
