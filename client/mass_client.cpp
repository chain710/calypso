#include "allocator.h"
#include "calypso_network.h"
#include "log_interface.h"
#include <tr1/functional>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

using namespace std;
using namespace std::tr1;
using namespace log4cplus;

const char* ECHO_STR = "hello, dear\r\n";

class massclients_t
{
public:
    typedef std::tr1::function<int (int, netlink_t&, unsigned int evt, void*)> onevent_callback;

    int initialize(int max_fd_num);
    void run();
    void cleanup();
    int on_net_event( int link_idx, netlink_t& link, unsigned int evt, void* up);
    int connect_to(const char* ip, unsigned short port);
private:
    dynamic_allocator_t allocator_;
    fixed_size_allocator_t* subs_;
    calypso_network_t network_;
    time_t nowtime_;
};

int massclients_t::initialize(int max_fd_num)
{
    int ret;
    const int allocator_num = 2;
    const int base_size = 1024;
    const int allocator_capacity = 128;

    int asize = base_size;
    // create allocator
    allocator_.initialize(allocator_num);
    subs_ = new fixed_size_allocator_t[allocator_num];
    for (int i = 0; i < allocator_num; ++i)
    {
        ret = subs_[i].initialize(asize, allocator_capacity);
        if (ret < 0)
        {
            C_FATAL("init fix alloc(%d,%d) failed", asize, allocator_capacity);
            return -1;
        }

        asize *= 2;
        ret = allocator_.add_allocator(subs_[i]);
        if (ret < 0)
        {
            C_FATAL("add allocator failed %d", ret);
            return -1;
        }
    }

    ret = network_.init(max_fd_num, max_fd_num, &allocator_);
    if (ret < 0)
    {
        C_FATAL("init network failed %d", ret);
        return -1;
    }

    nowtime_ = time(NULL);
    network_.refresh_nowtime(nowtime_);

    return 0;
}

void massclients_t::run()
{
    int evt_num;
    const int recover_num = 128;
    const int check_num = 128;
    const int max_tcp_idle = 60;
    const int connect_timeout = 5;

    massclients_t::onevent_callback onevent_func = std::tr1::bind(&massclients_t::on_net_event, this, tr1::placeholders::_1, tr1::placeholders::_2, tr1::placeholders::_3, tr1::placeholders::_4);
    while (true)
    {
        nowtime_ = time(NULL);

        network_.refresh_nowtime(nowtime_);
        // retry error connection
        network_.recover(recover_num);

        // check idle
        network_.check_idle_netlink(check_num, max_tcp_idle, connect_timeout);

        // wait network
        evt_num = network_.wait(onevent_func, NULL);

        if (0 == evt_num)
        {
            usleep(50000);
        }
    }
}

void massclients_t::cleanup()
{

}

int massclients_t::on_net_event( int link_idx, netlink_t& link, unsigned int evt, void* up )
{
    // 连接成功了，sendmsg
    // 收到echo打印，close, move to errorlist等待recover
    if (evt & error_event) return 0;

    string tmp;
    int fd = link.getfd();
    char addr_str[64];
    int ret;
    if (evt & connect_event)
    {
        ret = link.send(ECHO_STR, strlen(ECHO_STR));
        return 0;
    }

    if (evt & data_arrival_event)
    {
        ret = link.recv();
        if (ret < 0)
        {
            C_ERROR("link(fd:%d) recv failed", fd);
            link.close();
            return -1;
        }

        if (link.is_closed())
        {
            C_WARN("link(fd:%d) is closed by peer", fd);
        }

        int data_len = 0;
        char* data = link.get_recv_buffer(data_len);
        tmp.assign(data, data_len);
        C_DEBUG("recv %s(%d bytes) from %s", tmp.c_str(), data_len, link.get_remote_addr_str(addr_str, sizeof(addr_str)));

        if (string::npos != tmp.find(ECHO_STR))
        {
            link.close();
        }

        // dont pop, maybe fragment
    }

    return 0;
}

int massclients_t::connect_to(const char* ip, unsigned short port)
{
    netlink_config_t::config_item_t config;
    memset(&config, 0, sizeof(config));
    snprintf(config.ip_, sizeof(config.ip_), "%s", ip);
    config.port_ = port;
    config.sys_recv_buffer_ = 128;
    config.sys_send_buffer_ = 128;
    config.usr_recv_buffer_ = 128;
    config.usr_send_buffer_ = 128;
    snprintf(config.type_, sizeof(config.type_), "%s", "connect");
    return network_.create_link(config);
}

int main(int argc, char** argv)
{
    if (argc < 4)
    {
        printf("usage %s ip port num\n", argv[0]);
        return 0;
    }

    const char* ip = argv[1];
    unsigned short port = strtol(argv[2], NULL, 10);
    int max_fd_num = strtol(argv[3], NULL, 10);

    int ret;
    C_LOG_INST.setLogLevel(TRACE_LOG_LEVEL);
    SharedAppenderPtr myAppender(new ConsoleAppender());
    myAppender->setName("a1");
    C_LOG_INST.addAppender(myAppender);
    std::auto_ptr<Layout> myLayout = std::auto_ptr<Layout>(new log4cplus::PatternLayout(LOG4CPLUS_TEXT("[%-5p][%x][%l] %m%n")));
    myAppender->setLayout(myLayout);

    
    massclients_t runner;
    runner.initialize(max_fd_num+10);
    for (int i = 0; i < max_fd_num; ++i)
    {
        ret = runner.connect_to(ip, port);
        if (ret < 0)
        {
            C_ERROR("create connect[%d] failed", i);
        }
    }

    runner.run();

    return 0;
}
