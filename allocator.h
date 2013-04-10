#ifndef _CALYPSO_ALLOCATOR_H_
#define _CALYPSO_ALLOCATOR_H_

#include "linked_list.h"
#include "rbtree.h"

class fixed_size_allocator_t
{
public:
    fixed_size_allocator_t();
    virtual ~fixed_size_allocator_t();
    

    // size: 每块内存大小, capacity: 节点容量
    int initialize(int size, int capacity);
    char* get(int idx);
    int get_idx(char* p);
    char* alloc();
    int alloc2();
    int dealloc(char* buffer);
    int dealloc(int idx);
    int get_capacity() const { return capacity_; }
    int get_buffer_size() const { return size_; }
    int get_used_num() const { return buffer_list_->get_num(lused); }
    int get_free_num() const { return capacity_ - get_used_num(); }
private:
    enum list_id_t
    {
        lfree = 0,
        lused = 1,
    };

    struct buffer_node_t
    {
        unsigned int flag_;
        char data_[0];
    };

    // deny copy-cons
    fixed_size_allocator_t(const fixed_size_allocator_t& c) {}
    void init();
    void finalize();
    int init_buffer_node(int idx, buffer_node_t*& node, void* up);

    linked_list_t<buffer_node_t*>* buffer_list_;
    int size_;
    int capacity_;
    char* buffers_;
};

// 动态内存分配器，用红黑树组织, 分配出来的fixed_size_buffer_t要预留N个字节保存分配器的信息
class dynamic_allocator_t
{
public:
    struct buffer_addr_t
    {
        int asize_;
        int idx_;
    };

    dynamic_allocator_t();
    virtual ~dynamic_allocator_t();

    int initialize(int max_allocator_num);
    // 注意:添加的allocator实际可分配单元比原来小sizeof(buffer_head_t)，因为每块单元要存储额外信息
    int add_allocator(fixed_size_allocator_t& allocator);
    char* alloc(size_t s);
    buffer_addr_t alloc2(size_t s);
    // 重分配, 出错返回NULL, p依然可以使用，否则返回新地址
    char* realloc(int newsize, char* p);
    // 释放
    int dealloc(char* buffer);
    // 释放
    int dealloc(buffer_addr_t addr);
    // 根据buffer_addr_t返回真实地址
    char* get(buffer_addr_t addr);
private:
    const static int MAGIC_GUARD;

    struct buffer_head_t
    {
        int guard_;
        int asize_; // 分配器size, 实际可用要减去sizeof(buffer_head_t)
        char data_[0];
    };

    struct sub_allocator_t
    {
        fixed_size_allocator_t* allocator_;
    };

    struct _sub_allocator_cmp
    {
        int operator()(const int& l, const int& r)
        {
            return l - r;
        }
    };

    buffer_addr_t ptr2addr(char* p);
    inline buffer_head_t* ptr2head(char* p)
    {
        buffer_head_t* h = (buffer_head_t*)(p - offsetof(buffer_head_t, data_));
        return (h && MAGIC_GUARD == h->guard_)? h: NULL;
    }

    llrbtree_t<int, sub_allocator_t, _sub_allocator_cmp> allocators_;
};

#endif
