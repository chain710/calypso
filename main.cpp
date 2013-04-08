#include <log4cplus/logger.h>
#include <log4cplus/loggingmacros.h>
#include <log4cplus/configurator.h>
#include <log4cplus/consoleappender.h>
#include <log4cplus/layout.h>
#include <log4cplus/helpers/pointer.h>
#include <log4cplus/ndc.h>

#include <unistd.h>

using namespace log4cplus;
using namespace log4cplus::helpers;
using namespace std;

class app_handler_t
{
public:
    ~app_handler_t() {}
    virtual int on_recv_msgpack(int link_ident, int pack_type, void* pack) = 0;
};

int check_network(netlink_t* links, int link_num)
{
    int maxfd = -1;
    fd_set rdfds, wrfds;
    FD_ZERO(&rdfds);
    FD_ZERO(&wrfds);
    for (int i = 0; i < link_num; ++i)
    {
        FD_SET(links[i].getfd(), &rdfds);
        FD_SET(links[i].getfd(), &wrfds);
        if (maxfd < links[i].getfd())
        {
            maxfd = links[i].getfd();
        }
    }

    // select all netlinks
    timeval sel_timeout = {0, 0};
    int err =  select(maxfd, &rdfds, &wrfds, NULL, &sel_timeout);
    if (err < 0)
    {
        // log select error
        return -1;
    }
    
    if (err < 0)
    {
        // no socket event
        return 0;
    }

    for (int i = 0; i < link_num; ++i)
    {
        netlink_t& link = links[i];
        switch (link.get_status())
        {
        case nsc_established:
            // readable?
            if (FD_ISSET(link.getfd(), &rdfds))
            {
                err = link.recv();
                if (link.is_closed_by_peer())
                {
                    link.close();
                    // will try reconn later
                    continue;
                }
            }
            
            break;
        case nsc_connecting:
            // established?
            if (FD_ISSET(link.getfd(), &rdfds) || FD_ISSET(link.getfd(), wrfds))
            {
                if (getsockopt(link.getfd(), SOL_SOCKET, SO_ERROR, &err, sizeof(err)) < 0)
                {
                    // pending error
                    continue;
                }

                // connect succ
                LOG4CPLUS_INFO(, "connect xxx succ");
            }
            else
            {
                // connect timeout?
            }
            break;
        case nsc_listening:
            if (FD_ISSET(link.getfd(), &rdfds))
            {
                // accept
                LOG4CPLUS_INFO(, "accept new client from xxx, fd=");
            }
            break;
        default:
            // other bad status, try reconnect
            break;
        }
    }
}

int check_timer()
{
    // 选择不大于 time_til_end/2 的定时器重新计时
    // 定时器级别: 500ms 3s 30s 180s(3min) 30min
}

int main(int argc, char** argv)
{
    Logger logger = Logger::getInstance("main");

    SharedObjectPtr<Appender> append_1(new ConsoleAppender());
    append_1->setLayout( std::auto_ptr<Layout>(new PatternLayout("%d{%Y-%m-%d %H:%M:%S}|%p|%c(%x)|%l|%m%n")) );
    append_1->setName("console");
    logger.addAppender(append_1);

    //logger
    {
        NDCContextCreator ndc(LOG4CPLUS_TEXT("second"));
        LOG4CPLUS_WARN(logger, LOG4CPLUS_TEXT("Hello, World!"));
    }
    
    LOG4CPLUS_WARN(logger, LOG4CPLUS_TEXT("second log!!!!"));

    if (/**/)
    {
        int err = daemon(1, 0);
        if (err < 0)
        {
            LOG4CPLUS_FATAL(logger, "daemonize failed, errno=%d" << errno );
            exit(-1);
        }
    }
    
    return 0;
}
