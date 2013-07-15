#include "netlink.h"
#include "log_interface.h"
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstdio>
#include <cstddef>

const int DEFAULT_BUFFER_SIZE = 128;
const int EXPAND_NETLINK_BUFSIZE = 4096;

time_t netlink_t::now_time_ = 0;

netlink_t::netlink_t()
{
    fd_ = -1;
    status_ = nsc_closed;
    memset(&local_addr_, 0, sizeof(local_addr_));
    memset(&remote_addr_, 0, sizeof(remote_addr_));
    allocator_ = NULL;
    recv_buf_ = NULL;
    send_buf_ = NULL;
    last_active_time_ = 0;
    last_recover_time_ = 0;
    memset(&opt_, 0, sizeof(opt_));
    listen_backlog_ = 128;
    on_status_change_ = NULL;
}

netlink_t::~netlink_t()
{
    clear();
}

int netlink_t::init( link_opt_t opt, int fd )
{
    if (fd_ >= 0)
    {
        clear();
    }

    fd_ = fd;
    opt_ = opt;

    return setup();
}

int netlink_t::accept( netlink_t* child ) const
{
    sockaddr_in remote_addr;
    socklen_t addr_len = sizeof(remote_addr);
    int newfd = ::accept(fd_, (sockaddr*)&remote_addr, &addr_len);
    if (newfd < 0)
    {
        C_ERROR("accept(%d) error(%d)", fd_, errno);
        return -1;
    }

    if (child)
    {
        child->remote_addr_ = remote_addr;
        child->local_addr_ = local_addr_;
        child->allocator_ = allocator_;
        child->opt_ = opt_;
        child->opt_.ltype_ = accept_link;
        return child->init(child->opt_, newfd);
    }
    else
    {
        ::close(newfd);
        return 0;
    }
}

int netlink_t::bind( const char* ip, unsigned short port )
{
    if (opt_.ltype_ == accept_link)
    {
        C_ERROR("accept link(%d) cant bind local addr", fd_);
        return -1;
    }

    set_bind_addr(ip, port);
    return ::bind(fd_, (sockaddr*)&local_addr_, sizeof(local_addr_));
}

int netlink_t::listen( int backlog )
{
    if (opt_.ltype_ != server_link)
    {
        C_ERROR("this link(type=%d) cant listen", opt_.ltype_);
        return -1;
    }

    last_active_time_ = now_time_;
    set_listen_backlog(backlog);
    int ret = ::listen(fd_, listen_backlog_);
    if (ret < 0)
    {
        C_ERROR("listen(%d) error(%d)", fd_, errno);
        return -1;
    }

    return mod_status(nsc_listening);
}

int netlink_t::connect( const char* ip, unsigned short port )
{
    if (opt_.ltype_ != client_link || nsc_open != status_ || NULL == ip || 0 == port)
    {
        C_ERROR("invalid link type(%d) or status(%d) or param(ip=%p port=%d)", 
            opt_.ltype_, status_, ip, port);
        return -1;
    }

    set_remote_addr(ip, port);
    return do_connect();
}

int netlink_t::setup()
{
    int err = -1;
    last_active_time_ = now_time_;
    do 
    {
        int ret;
        int sock_status;
        if (fd_ < 0)
        {
            fd_ = socket(AF_INET, SOCK_STREAM, 0);
            if (fd_ < 0)
            {
                C_ERROR("new socket error(%d)", errno);
                break;
            }

            sock_status = nsc_open;
        }
        else
        {
            sock_status = nsc_established;
        }

        ret = add_fd_flag(O_NONBLOCK);
        if (ret < 0) break;
        ret = mod_status(sock_status);
        if (ret < 0) break;

        if (server_link != opt_.ltype_)
        {
            // 设置系统缓冲区大小
            ret = set_sock_buffer_size(opt_.sys_sndbuf_size_, opt_.sys_rcvbuf_size_);
            if (ret < 0)
            {
                break;
            }

            // 申请用户缓冲区
            if (NULL == recv_buf_)
            {
                opt_.usr_rcvbuf_size_ = opt_.usr_rcvbuf_size_ > 0? opt_.usr_rcvbuf_size_: DEFAULT_BUFFER_SIZE;
                recv_buf_ = (link_buffer_t*)allocator_->alloc(opt_.usr_rcvbuf_size_ + sizeof(link_buffer_t));
                if (NULL == recv_buf_)
                {
                    C_ERROR("alloc recvbuf(%d) failed", opt_.usr_rcvbuf_size_ + (int)sizeof(link_buffer_t));
                    break;
                }
            }

            recv_buf_->used_ = 0;

            if (NULL == send_buf_)
            {
                opt_.usr_sndbuf_size_ = opt_.usr_sndbuf_size_ > 0? opt_.usr_sndbuf_size_: DEFAULT_BUFFER_SIZE;
                send_buf_ = (link_buffer_t*)allocator_->alloc(opt_.usr_sndbuf_size_ + sizeof(link_buffer_t));
                if (NULL == send_buf_)
                {
                    C_ERROR("alloc sendbuf(%d) failed", opt_.usr_rcvbuf_size_ + (int)sizeof(link_buffer_t));
                    break;
                }
            }

            send_buf_->used_ = 0;

            // linger?
            /* 
            This option specifies how the close function operates for a connection-oriented protocol (e.g., for TCP and SCTP, but not for UDP). 
            By default, close returns immediately, but if there is any data still remaining in the socket send buffer, the system will try to deliver the data to the peer.
            */

            // keepalive?
            if (opt_.flag_ & lf_keepalive)
            {
                int keep_alive = 1;
                if (setsockopt(fd_, SOL_SOCKET, SO_KEEPALIVE, &keep_alive, sizeof(keep_alive)) < 0)
                {
                    C_ERROR("set sock(%d) keepalive error(%d)", fd_, errno);
                    break;
                }
            }
        }
        else
        {
            // reuseaddr?
            if (opt_.flag_ & lf_reuseaddr)
            {
                int reuse_addr = 1;
                if (setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr)) < 0)
                {
                    C_ERROR("set sock(%d) reuseaddr error(%d)", fd_, errno);
                    break;
                }
            }
        }

        err = 0;
    } while (false);

    if (err < 0)
    {
        close();
        return -1;
    }

    return 0;
}

int netlink_t::add_fd_flag( int flags )
{
    int f = fcntl(fd_, F_GETFL, 0);
    if (f < 0)
    {
        C_ERROR("get fd(%d) flag error(%d)", fd_, errno);
        return -1;
    }

    f |= flags;
    f = fcntl(fd_, F_SETFL, f);
    if (f < 0)
    {
        C_ERROR("set fd(%d) flag(%d) error(%d)", fd_, f, errno);
        return -1;
    }

    return 0;
}

int netlink_t::set_sock_buffer_size( int snd, int rcv )
{
    int optval;
    int optlen = sizeof(optval);
    int ret;
    if (snd > 0)
    {
        optval = snd;
        ret = setsockopt(fd_, SOL_SOCKET, SO_SNDBUF, &optval, optlen);
        if (ret < 0)
        {
            C_ERROR("set fd(%d) sendbuf(%d) error(%d)", fd_, snd, errno);
            return -1;
        }
    }

    if (rcv > 0)
    {
        optval = rcv;
        ret = setsockopt(fd_, SOL_SOCKET, SO_RCVBUF, &optval, optlen);
        if (ret < 0)
        {
            C_ERROR("set fd(%d) recvbuf(%d) error(%d)", fd_, rcv, errno);
            return -1;
        }
    }

    return 0;
}

int netlink_t::do_connect()
{
    int ret;
    last_active_time_ = now_time_;
    do
    {
        ret = ::connect(fd_, (const sockaddr*)&remote_addr_, sizeof(remote_addr_));
        if (0 == ret)
        {
            // this can occur when the server is on the same host as the client
            return mod_status(nsc_established);
        }
        else if (EINPROGRESS == errno)
        {
            return mod_status(nsc_connecting);
        }
        else if (EINTR != errno)
        {
            char remote_addr_str[64];
            C_ERROR("connect(%d, %s) error(%d)", fd_, get_remote_addr_str(remote_addr_str, sizeof(remote_addr_str)), errno);
            return -1;
        }
    } while (true);

    // A TCP socket is writable if there is available space in the send buffer (which will always be the case for a connecting socket since we have not yet written anything to the socket) and the socket is connected (which occurs only when the three-way handshake completes). A pending error causes a socket to be both readable and writable.
    return 0;
}

int netlink_t::recv()
{
    if (opt_.usr_rcvbuf_size_ <= 0)
    {
        C_ERROR("empty user recvbuf(%d), cant recv", opt_.usr_rcvbuf_size_);
        return -1;
    }

    last_active_time_ = now_time_;
    int ret, rlen = 0;
    char addr_str[64];
    while (opt_.usr_rcvbuf_size_ > recv_buf_->used_) 
    {
        ret = ::recv(fd_, &recv_buf_->data_[recv_buf_->used_], opt_.usr_rcvbuf_size_ - recv_buf_->used_, 0);
        if (ret > 0)
        {
            C_DEBUG("recv %d bytes from %s", ret, get_remote_addr_str(addr_str, sizeof(addr_str)));
            recv_buf_->used_ += ret;
            // extend
            rlen += ret;
            if (recv_buf_->used_ == opt_.usr_rcvbuf_size_)
            {
                int ext_len = (opt_.usr_rcvbuf_size_ < EXPAND_NETLINK_BUFSIZE)? (opt_.usr_rcvbuf_size_*2): (recv_buf_->used_+EXPAND_NETLINK_BUFSIZE);
                char* newbuf = allocator_->realloc(ext_len+sizeof(link_buffer_t), (char*)recv_buf_);
                if (NULL == newbuf)
                {
                    // NOTE: 两种处理方案
                    // 1.return -1 提示内存不足，该链路必须被关闭
                    // 2.return 0 剩下的buf留在系统缓冲里。如果是ET模式且该链路不再发送数据，那么系统缓冲里的数据就一直无法被读取，除非在netlink结构里加上标识指示该链路的系统缓冲还有数据待接收
                    C_ERROR("no more recvbuf, current size %d", opt_.usr_rcvbuf_size_);
                    return -1;
                }

                recv_buf_ = (link_buffer_t*)newbuf;
                opt_.usr_rcvbuf_size_ = ext_len;
            }
        }
        else if (0 == ret)
        {
            // 这里仅关闭链路，不归还缓存，以免调用者希望recover
            do_close();
            break;
        }
        else if (EAGAIN == errno)
        {
            // no data
            break;
        }
        else if (EINTR != errno)
        {
            // recv error
            C_ERROR("recv(%d) error(%d)", fd_, errno);
            return -1;
        }
    }
    
    return rlen;
}

int netlink_t::copy_data_to_send_buffer(const char* buffer, int len)
{
    if (NULL == buffer) return 0;
    if (send_buf_->used_ + len > opt_.usr_sndbuf_size_)
    {
        int newbuf_len = send_buf_->used_ + len;
        char* newbuf = allocator_->realloc(newbuf_len + sizeof(link_buffer_t), (char*)send_buf_);
        if (NULL == newbuf)
        {
            // send buf overflow
            C_ERROR("no more sendbuf, current size %d", opt_.usr_sndbuf_size_);
            return -1;
        }

        send_buf_ = (link_buffer_t*) newbuf;
        opt_.usr_sndbuf_size_ = newbuf_len;
    }

    memcpy(&send_buf_->data_[send_buf_->used_], buffer, len);
    send_buf_->used_ += len;
    return 0;
}

int netlink_t::send( const char* buf, int len )
{
    int ret;
    int send_len = 0;
    last_active_time_ = now_time_;
    // send remain
    while (send_buf_->used_ > send_len) 
    {
        ret = ::send(fd_, send_buf_->data_ + send_len, send_buf_->used_ - send_len, MSG_NOSIGNAL);
        if (ret >= 0)
        {
            send_len += ret;
        }
        else if (EAGAIN == errno)
        {
            // move mem
            if (send_len > 0)
            {
                memmove(send_buf_->data_, &send_buf_->data_[send_len], send_buf_->used_ - send_len);
                send_buf_->used_ -= send_len;
            }

            // copy buf:len
            return copy_data_to_send_buffer(buf, len);
        }
        else if (EINTR != errno)
        {
            C_ERROR("send(%d) error(%d)", fd_, errno);
            return -1;
        }
    }

    send_buf_->used_ = 0;
    send_len = 0;
    while (buf && len > send_len) 
    {
        ret = ::send(fd_, buf + send_len, len - send_len, MSG_NOSIGNAL);
        if (ret >= 0)
        {
            send_len += ret;
        }
        else if (EAGAIN == errno)
        {
            return copy_data_to_send_buffer(buf + send_len, len - send_len);
        }
        else if (EINTR != errno)
        {
            C_ERROR("send(%d) error(%d)", fd_, errno);
            return -1;
        }
    }

    return 0;
}

int netlink_t::close()
{
    do_close();
    return 0;
}

int netlink_t::clear()
{
    int err = 0;
    do_close();
    memset(&local_addr_, 0, sizeof(local_addr_));
    memset(&remote_addr_, 0, sizeof(remote_addr_));

    if (recv_buf_)
    {
        int ret = allocator_->dealloc((char*)recv_buf_);
        if (ret < 0) err = -1;
        recv_buf_ = NULL;
    }
    
    if (send_buf_)
    {
        int ret = allocator_->dealloc((char*)send_buf_);
        if (ret < 0) err = -1;
        send_buf_ = NULL;
    }

    last_recover_time_ = 0;

    return err;
}

int netlink_t::recover()
{
    if (client_link == opt_.ltype_ || server_link == opt_.ltype_)
    {
        last_recover_time_ = now_time_;
        do_close();
        if (setup() < 0) return -1;
        return configure();
    }

    C_ERROR("this link(type=%d) cant recover", opt_.ltype_);
    return -1;
}

void netlink_t::do_close()
{
    mod_status(nsc_closed);
    if (fd_ > 0)
    {
        ::close(fd_);
        fd_ = -1;
    }
}

char* netlink_t::get_recv_buffer( int& len )
{
    if (NULL == recv_buf_)
    {
        return NULL;
    }

    len = recv_buf_->used_;
    return recv_buf_->data_;
}

int netlink_t::pop_recv_buffer( int len )
{
    if (NULL == recv_buf_)
    {
        return 0;
    }

    if (len < 0 || len > recv_buf_->used_)
    {
        len = recv_buf_->used_;
    }

    if (recv_buf_->used_ - len > 0)
    {
        memmove(recv_buf_->data_, &recv_buf_->data_[len], recv_buf_->used_ - len);
    }

    recv_buf_->used_ -= len;
    return len;
}

const char* netlink_t::get_local_addr_str( char* buf, int size ) const
{
    sockaddr_in laddr;
    socklen_t len = sizeof(laddr);
    int ret = getsockname(fd_, (sockaddr *)&laddr, &len);
    if (ret < 0)
    {
        return NULL;
    }

    snprintf(buf, size, "%s:%d", inet_ntoa(laddr.sin_addr), ntohs(laddr.sin_port));
    return buf;
}

const char* netlink_t::get_remote_addr_str( char* buf, int size ) const
{
    snprintf(buf, size, "%s:%d", inet_ntoa(remote_addr_.sin_addr), ntohs(remote_addr_.sin_port));
    return buf;
}

int netlink_t::get_sock_error() const
{
    int sock_err = 0;
    socklen_t errlen = sizeof(sock_err);
    int ret = getsockopt(fd_, SOL_SOCKET, SO_ERROR, &sock_err, &errlen);
    if (ret < 0)
    {
        C_ERROR("get sock(%d) errno error(%d)", fd_, errno);
        return -1;
    }

    return sock_err;
}

int netlink_t::set_established()
{
    if (nsc_connecting == status_ && fd_ >= 0)
    {
        return mod_status(nsc_established);
    }

    C_ERROR("this link(fd=%d status=%d) cant set established", fd_, status_);
    return -1;
}

int netlink_t::mod_status( int status )
{
    if (status == status_)
    {
        return 0;
    }

    status_ = status;
    if (on_status_change_ != NULL)
    {
        return on_status_change_(*this);
    }

    return 0;
}

void netlink_t::set_bind_addr( const char* ip, unsigned short port )
{
    local_addr_.sin_family = AF_INET;
    local_addr_.sin_port = htons(port);
    if (ip && ip[0] != '\0')
    {
        local_addr_.sin_addr.s_addr = inet_addr(ip);
    }
    else
    {
        local_addr_.sin_addr.s_addr = htonl(INADDR_ANY);
    }
}

void netlink_t::set_listen_backlog( int backlog )
{
    listen_backlog_ = backlog;
}

void netlink_t::set_remote_addr( const char* ip, unsigned short port )
{
    remote_addr_.sin_family = AF_INET;
    remote_addr_.sin_port = htons(port);
    remote_addr_.sin_addr.s_addr = inet_addr(ip);
}

int netlink_t::configure()
{
    if (AF_INET == local_addr_.sin_family)
    {
        int ret = ::bind(fd_, (sockaddr*)&local_addr_, sizeof(local_addr_));
        if (ret < 0)
        {
            char laddrstr[64];
            snprintf(laddrstr, sizeof(laddrstr), "%s:%d", inet_ntoa(local_addr_.sin_addr), ntohs(local_addr_.sin_port));
            C_ERROR("bind(%d) local addr(%s) error(%d)", fd_, laddrstr, errno);
            return -1;
        }
    }

    switch (opt_.ltype_)
    {
    case client_link:
        return do_connect();
    case server_link:
        return listen(listen_backlog_);
    default:
        return 0;
    }
}

void netlink_t::update_opt( link_opt_t opt )
{
    if (opt.ltype_ != opt_.ltype_)
    {
        C_FATAL("link fd:%d type has changed, MEMORY CORRUPT? old=%d new=%d, this link WILL NOT be updated", fd_, opt_.ltype_, opt.ltype_);
        return;
    }

    opt_.mask_ = opt.mask_;
    opt_.flag_ = opt.flag_;
    int err = set_sock_buffer_size(opt.sys_sndbuf_size_, opt.sys_rcvbuf_size_);
    if (0 == err)
    {
        opt_.sys_rcvbuf_size_ = opt.sys_rcvbuf_size_;
        opt_.sys_sndbuf_size_ = opt.sys_sndbuf_size_;
    }

    if (server_link == opt.ltype_)
    {
        // only listening socket can update usr*bufsize
        if (opt.usr_rcvbuf_size_ > 0) opt_.usr_rcvbuf_size_ = opt.usr_rcvbuf_size_;
        if (opt.usr_sndbuf_size_ > 0) opt_.usr_sndbuf_size_ = opt.usr_sndbuf_size_;
    }
}
