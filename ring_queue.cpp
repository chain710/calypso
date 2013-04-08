#include "ring_queue.h"
#include <assert.h>
#include <string.h>

#define RING_QUEUE_STX (0x02)
#define RING_QUEUE_ETX (0x03)

ring_queue_t::ring_queue_t()
{
    buffer_ = NULL;
    buffer_len_ = 0;
    read_ = -1;
    write_ = -1;
}

ring_queue_t::ring_queue_t( const ring_queue_t& c )
{
    buffer_len_ = c.buffer_len_;
    buffer_ = new char[buffer_len_];
    assert(NULL != buffer_);
    read_ = c.read_;
    write_ = c.write_;
    memcpy(buffer_, c.buffer_, buffer_len_);
}

ring_queue_t::~ring_queue_t()
{
    finalize();
}

int ring_queue_t::initialize( int len )
{
    if (len <= 0)
    {
        return -1;
    }

    finalize();
    buffer_ = new char[len];
    if (NULL == buffer_)
    {
        return -1;
    }

    buffer_len_ = len;
    read_ = 0;
    write_ = 0;

    return 0;
}

void ring_queue_t::finalize()
{
    if (buffer_)
    {
        delete []buffer_;
        buffer_ = NULL;
    }

    buffer_len_ = 0;
}

int ring_queue_t::produce( const char* data, int len )
{
    if (get_free_len() < len + (int)sizeof(ring_block_t))
    {
        // out of space
        return -1;
    }

    ring_block_t rb;
    rb.len_ = len;
    rb.stx_ = RING_QUEUE_STX;
    produce_bytes(write_, (char*)&rb, sizeof(rb));
    produce_bytes(write_ + sizeof(rb), data, len);
    produce_finish();
    return 0;
}

int ring_queue_t::consume( char* data, int len )
{
    int clen = get_consume_len();
    if (clen <= 0) return clen;

    if (len < clen)
    {
        return -1;
    }

    skip_consume_bytes(sizeof(ring_block_t));
    consume_bytes(data, clen);
    return clen;
}

int ring_queue_t::get_consume_len() const
{
    if (get_used_len() <= 0)
    {
        return 0;
    }

    ring_block_t rb;
    extract_rb(rb, read_);
    if (rb.stx_ != RING_QUEUE_STX)
    {
        return -1;
    }

    if (get_used_len() < (int)rb.len_ + (int)sizeof(ring_block_t))
    {
        return -1;
    }

    return rb.len_;
}

int ring_queue_t::get_free_len() const
{
    if (write_ >= read_)
    {
        return buffer_len_ - (write_ - read_) - 1;
    }
    else
    {
        return read_ - write_ - 1;
    }
}

int ring_queue_t::get_used_len() const
{
    return buffer_len_ - 1 - get_free_len();
}

void ring_queue_t::produce_bytes( int pos, const char* data, int len )
{
    pos = normalize_pos(pos);
    if (pos + len > buffer_len_)
    {
        int backlen = buffer_len_ - pos;
        memcpy(&buffer_[pos], data, backlen);
        memcpy(&buffer_[0], &data[backlen], len - backlen);
    }
    else
    {
        memcpy(&buffer_[pos], data, len);
    }
}

void ring_queue_t::consume_bytes( char* data, int len )
{
    if (0 == len) return;
    if (read_ + len > buffer_len_)
    {
        int backlen = buffer_len_ - read_;
        memcpy(data, &buffer_[read_], backlen);
        memcpy(&data[backlen], buffer_, len - backlen);
        read_ = len - backlen;
    }
    else
    {
        memcpy(data, &buffer_[read_], len);
        read_ += len;
        if (read_ == buffer_len_) read_ = 0;
    }
}

void ring_queue_t::extract_rb( ring_block_t& rb, int from ) const
{
    char* data = (char*)&rb;
    int len = sizeof(rb);
    if (from + len > buffer_len_)
    {
        int backlen = buffer_len_ - from;
        memcpy(data, &buffer_[from], backlen);
        memcpy(&data[backlen], buffer_, len - backlen);
    }
    else
    {
        memcpy(data, &buffer_[from], len);
    }
}

void ring_queue_t::clear()
{
    read_ = 0;
    write_ = 0;
}

int ring_queue_t::produce_reserve(int len)
{
    if (get_free_len() < (int)sizeof(ring_block_t) + len)
    {
        // out of space
        return -1;
    }

    ring_block_t rb;
    rb.len_ = 0;
    rb.stx_ = RING_QUEUE_STX;
    produce_bytes(write_, (char*)&rb, sizeof(rb));
    return 0;
}

int ring_queue_t::produce_append( const char* data, int len )
{
    if (NULL == data || len < 0)
    {
        produce_finish();
        return 0;
    }

    ring_block_t rb;
    extract_rb(rb, write_);
    if (rb.stx_ != RING_QUEUE_STX)
    {
        return -1;
    }

    if (get_free_len() < len + (int)rb.len_ + (int)sizeof(rb))
    {
        return -1;
    }

    produce_bytes(write_ + sizeof(rb) + rb.len_, data, len);
    rb.len_ += len;
    produce_bytes(write_, (char*)&rb, sizeof(rb));
    return 0;
}

void ring_queue_t::produce_finish()
{
    ring_block_t rb;
    extract_rb(rb, write_);
    write_ = normalize_pos(write_ + rb.len_ + sizeof(rb));
}

void ring_queue_t::skip_consume_bytes( int len )
{
    if (len <= 0) return;
    read_ = normalize_pos(read_ + len);
}

int ring_queue_t::skip_consume()
{
    int clen = get_consume_len();
    if (clen <= 0) return clen;
    skip_consume_bytes(sizeof(ring_block_t));
    skip_consume_bytes(clen);
    return clen;
}
