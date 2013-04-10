#ifndef _CALYPSO_ALLOCATOR_H_
#define _CALYPSO_ALLOCATOR_H_

#include "linked_list.h"
#include "rbtree.h"

class fixed_size_allocator_t
{
public:
    fixed_size_allocator_t();
    virtual ~fixed_size_allocator_t();
    

    // size: ÿ���ڴ��С, capacity: �ڵ�����
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

// ��̬�ڴ���������ú������֯, ���������fixed_size_buffer_tҪԤ��N���ֽڱ������������Ϣ
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
    // ע��:��ӵ�allocatorʵ�ʿɷ��䵥Ԫ��ԭ��Сsizeof(buffer_head_t)����Ϊÿ�鵥ԪҪ�洢������Ϣ
    int add_allocator(fixed_size_allocator_t& allocator);
    char* alloc(size_t s);
    buffer_addr_t alloc2(size_t s);
    // �ط���, ������NULL, p��Ȼ����ʹ�ã����򷵻��µ�ַ
    char* realloc(int newsize, char* p);
    // �ͷ�
    int dealloc(char* buffer);
    // �ͷ�
    int dealloc(buffer_addr_t addr);
    // ����buffer_addr_t������ʵ��ַ
    char* get(buffer_addr_t addr);
private:
    const static int MAGIC_GUARD;

    struct buffer_head_t
    {
        int guard_;
        int asize_; // ������size, ʵ�ʿ���Ҫ��ȥsizeof(buffer_head_t)
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
