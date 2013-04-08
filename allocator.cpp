#include "allocator.h"
#include "log_interface.h"
#include <cstring>
#include <stddef.h>

using namespace std;
using namespace std::tr1;

const int dynamic_allocator_t::MAGIC_GUARD = 0x343;

enum buffer_flag_t
{
    buffer_used = 0x00000001,
};

fixed_size_allocator_t::fixed_size_allocator_t()
{
    init();
}

fixed_size_allocator_t::~fixed_size_allocator_t()
{
    finalize();
}

int fixed_size_allocator_t::initialize( int size, int capacity )
{
    size_ = size;
    capacity_ = capacity;
    buffers_ = new char[capacity*(size+sizeof(buffer_node_t))];
    buffer_list_ = new linked_list_t<buffer_node_t*>(capacity);
    if (NULL == buffers_ || NULL == buffer_list_)
    {
        C_ERROR("create buffer(%p) or list(%p) failed, oom?", buffers_, buffer_list_);
        return -1;
    }

    buffer_list_->walk_list(NULL, bind(&fixed_size_allocator_t::init_buffer_node, this, placeholders::_1, placeholders::_2, placeholders::_3));
    return 0;
}

char* fixed_size_allocator_t::alloc()
{
    return get(alloc2());
}

int fixed_size_allocator_t::alloc2()
{
    int ret = buffer_list_->append(used_list_);
    if (ret < 0)
    {
        C_ERROR("bufferlist usedlist(%d) append error, oom?", used_list_.num_);
        return -1;
    }

    buffer_node_t** b = buffer_list_->get(used_list_.tail_);
    (*b)->flag_ |= buffer_used;
    return used_list_.tail_;
}

int fixed_size_allocator_t::dealloc( char* buffer )
{
    int idx = buffer_list_->get_idx((buffer_node_t**)(buffer - offsetof(buffer_node_t, data_)));
    return dealloc(idx);
}

int fixed_size_allocator_t::dealloc( int idx )
{
    buffer_node_t** b = buffer_list_->get(idx);
    if (NULL == b || 0 == ((*b)->flag_ & buffer_used))
    {
        // not used yet
        C_ERROR("cant dealloc block[%d] cauz its not used", idx);
        return -1;
    }

    int ret = buffer_list_->swap(used_list_, idx, used_list_.head_);
    if (ret < 0)
    {
        // swap fail, maybe invalid buffer?
        C_ERROR("bufferlist swap error, idx:%d usedhead:%d", idx, used_list_.head_);
        return -1;
    }

    (*b)->flag_ &= (unsigned int)(~buffer_used);
    buffer_list_->remove(used_list_);
    return 0;
}

int fixed_size_allocator_t::init_buffer_node( int idx, buffer_node_t*& node, void* up )
{
    node = (buffer_node_t*)&buffers_[idx*(size_+sizeof(buffer_node_t))];
    node->flag_ = 0;
    return idx;
}

void fixed_size_allocator_t::finalize()
{
    if (buffer_list_) delete buffer_list_;
    if (buffers_) delete []buffers_;
    init();
}

void fixed_size_allocator_t::init()
{
    buffer_list_ = NULL;
    buffers_ = NULL;
    size_ = 0;
    capacity_ = 0;
    memset(&used_list_, 0, sizeof(used_list_));
    used_list_.head_ = -1;
    used_list_.tail_ = -1;
}

char* fixed_size_allocator_t::get( int idx )
{
    buffer_node_t** b = buffer_list_->get(idx);
    if (b && ((*b)->flag_ & buffer_used))
    {
        return (*b)->data_;
    }

    return NULL;
}

int fixed_size_allocator_t::get_idx( char* p )
{
    return (p - buffers_ - offsetof(buffer_node_t, data_)) / (size_ + sizeof(buffer_node_t));
}


dynamic_allocator_t::dynamic_allocator_t()
{

}

dynamic_allocator_t::~dynamic_allocator_t()
{
}

int dynamic_allocator_t::initialize( int max_allocator_num )
{
    return allocators_.initialize(max_allocator_num);
}

int dynamic_allocator_t::add_allocator( fixed_size_allocator_t& allocator )
{
    if (allocator.get_buffer_size() <= (int)sizeof(buffer_head_t))
    {
        C_ERROR("allocator unit too small, expect at least %d", (int)sizeof(buffer_head_t));
        return -1;
    }

    sub_allocator_t n;
    n.allocator_ = &allocator;
    return allocators_.insert(allocator.get_buffer_size(), n);
}

char* dynamic_allocator_t::alloc( size_t s )
{
    // find best size
    buffer_head_t* m = NULL;
    sub_allocator_t* a;
    while (NULL == m)
    {
        a = allocators_.get_ceiling(s+sizeof(buffer_head_t));
        if (NULL == a)
        {
            return NULL;
        }

        m = (buffer_head_t*)a->allocator_->alloc();
        if (NULL == m)
        {
            // try next
            s = a->allocator_->get_buffer_size() + 1;
        }
    }

    m->guard_ = MAGIC_GUARD;
    m->asize_ = a->allocator_->get_buffer_size();
    return m->data_;
}

dynamic_allocator_t::buffer_addr_t dynamic_allocator_t::alloc2( size_t s )
{
    return ptr2addr(alloc(s));
}

char* dynamic_allocator_t::realloc( int newsize, char* p )
{
    buffer_head_t* h = ptr2head(p);
    if (NULL == h)
    {
        return NULL;
    }

    if (h->asize_-(int)sizeof(buffer_head_t) >= newsize)
    {
        return p;
    }

    char* n = alloc(newsize);
    if (NULL == n)
    {
        return NULL;
    }

    memcpy(n, h->data_, h->asize_-sizeof(buffer_head_t));
    int ret = dealloc(p);
    if (ret < 0)
    {
        // may cause memleak
        dealloc(n);
        return NULL;
    }

    return n;
}

int dynamic_allocator_t::dealloc( char* buffer )
{
    return dealloc(ptr2addr(buffer));
}

int dynamic_allocator_t::dealloc( buffer_addr_t addr )
{
    sub_allocator_t* a = allocators_.get_by_key(addr.asize_);
    if (NULL == a)
    {
        C_ERROR("find no suballocator by %d", addr.asize_);
        return -1;
    }

    return a->allocator_->dealloc(addr.idx_);
}

char* dynamic_allocator_t::get( buffer_addr_t addr )
{
    sub_allocator_t* a = allocators_.get_by_key(addr.asize_);
    if (NULL == a)
    {
        return NULL;
    }

    return a->allocator_->get(addr.idx_);
}

dynamic_allocator_t::buffer_addr_t dynamic_allocator_t::ptr2addr( char* p )
{
    buffer_addr_t ret;
    ret.asize_ = 0;
    ret.idx_ = -1;
    if (NULL == p)
    {
        return ret;
    }

    buffer_head_t* h = ptr2head(p);
    if (NULL == h)
    {
        // guard fail
        return ret;
    }

    sub_allocator_t* a = allocators_.get_by_key(h->asize_);
    if (NULL == a)
    {
        return ret;
    }

    ret.asize_ = h->asize_;
    ret.idx_ = a->allocator_->get_idx((char*)h);
    return ret;
}
