#ifndef _LINKED_LIST_
#define _LINKED_LIST_

#include <cstddef>
#include <tr1/functional>
#include <string.h>

struct linked_list_flag_t
{
    int head_;
    int tail_;
    int num_;
};

inline void empty_list_flag(linked_list_flag_t& flag)
{
    memset(&flag, 0, sizeof(flag));
    flag.head_ = -1;
    flag.tail_ = -1;
}

template<typename T>
class linked_list_t
{
public:
    struct list_node_t
    {
        int prev_;
        int next_;
    };

    typedef std::tr1::function<int (int idx, T& node, void*)> walk_list_callback;
    typedef std::tr1::function<int (int idx, const T& node, void*)> walk_list_const_callback;

    linked_list_t(int max_length);
    virtual ~linked_list_t();
    
    // 交换两个元素在链表中的顺序
    int swap(linked_list_flag_t& flag, int left, int right);
    // move node from flag(head) to list(tail)
    int remove(linked_list_flag_t& flag);
    // move node from list(head) to flag(tail)
    int append(linked_list_flag_t& flag);
    // move node from src.head to dst.tail
    int move(linked_list_flag_t& src, linked_list_flag_t& dst);
    // move src(flag) after dst
    int move_after(linked_list_flag_t& flag, int src, int dst);
    // move src(flag) before dst
    int move_before( linked_list_flag_t& flag, int src, int dst );

    linked_list_flag_t get_flag() const { return flag_; }
    linked_list_flag_t& get_ref_flag() { return flag_; }

    void walk_list(linked_list_flag_t flag, void* up, walk_list_callback callback);
    void walk_list(linked_list_flag_t flag, void* up, walk_list_const_callback callback) const;
    void walk_list(void* up, walk_list_callback callback);
    void walk_list(int max_walk, linked_list_flag_t flag, void* up, walk_list_callback callback);
    T* get(int idx);
    const T* get(int idx) const;
    int get_idx(const T* data) const;
    int get_next(const T* data) const;
    int get_prev(const T* data) const;
    void reset();
    int get_length() const { return list_length_; }
private:
    linked_list_t(const linked_list_t& l){}
    void init_list();
    void init_node(list_node_t& n) { n.prev_ = -1; n.next_ = -1; }
    bool is_valid_index(int i) const { return i >= 0 && i < list_length_; }
    linked_list_flag_t flag_;
    list_node_t* nodes_;
    T* data_;
    int list_length_;
};

template<typename T>
linked_list_t<T>::linked_list_t( int max_length )
{
    nodes_ = new list_node_t[max_length];
    data_ = new T[max_length];
    list_length_ = max_length;
    init_list();
}

template<typename T>
linked_list_t<T>::~linked_list_t()
{
    delete []nodes_;
    list_length_ = 0;
    empty_list_flag(flag_);
    data_ = NULL;
    nodes_ = NULL;
}

template<typename T>
int linked_list_t<T>::swap( linked_list_flag_t& flag, int left, int right )
{
    if (right == left)
    {
        return 0;
    }

    if (!is_valid_index(left) || !is_valid_index(right))
    {
        return -1;
    }

    int lp = nodes_[left].prev_;
    int ln = nodes_[left].next_;
    int rp = nodes_[right].prev_;
    int rn = nodes_[right].next_;
    if (is_valid_index(lp)) nodes_[lp].next_ = right;
    if (is_valid_index(ln)) nodes_[ln].prev_ = right;
    if (is_valid_index(rp)) nodes_[rp].next_ = left;
    if (is_valid_index(rn)) nodes_[rn].prev_ = left;

    nodes_[left].prev_ = (left == rp)? right: rp;
    nodes_[left].next_ = (left == rn)? right: rn;
    nodes_[right].prev_ = (right == lp)? left: lp;
    nodes_[right].next_ = (right == ln)? left: ln;
    if (left == flag.head_)
    {
        flag.head_ = right;
    }
    else if (right == flag.head_)
    {
        flag.head_ = left;
    }

    if (left == flag.tail_)
    {
        flag.tail_ = right;
    }
    else if (right == flag.tail_)
    {
        flag.tail_ = left;
    }
    
    return 0;
}

template<typename T>
int linked_list_t<T>::remove( linked_list_flag_t& flag )
{
    return move(flag, flag_);
}

template<typename T>
int linked_list_t<T>::append( linked_list_flag_t& flag )
{
    return move(flag_, flag);
}

template<typename T>
int linked_list_t<T>::move( linked_list_flag_t& src, linked_list_flag_t& dst )
{
    if (!is_valid_index(src.head_) || dst.tail_ >= list_length_)
    {
        return -1;
    }

    // remove it from src.head
    int midx = src.head_;
    list_node_t& mnode = nodes_[midx];

    if (mnode.next_ < 0)
    {
        // empty
        empty_list_flag(src);
    }
    else
    {
        nodes_[mnode.next_].prev_ = -1;
        src.head_ = mnode.next_;
        --src.num_;
    }

    // append it to dst.tail
    if (dst.tail_ < 0)
    {
        // empty, dst.head_ must < 0
        dst.head_ = midx;
    }
    else
    {
        nodes_[dst.tail_].next_ = midx;
    }

    mnode.prev_ = dst.tail_;
    mnode.next_ = -1;
    dst.tail_ = midx;
    ++dst.num_;
    return 0;
}

template<typename T>
void linked_list_t<T>::walk_list( linked_list_flag_t flag, void* up, walk_list_callback callback )
{
    // FIXME: 如果在callback里修改list?
    int ret;
    for (int i = flag.head_; i >= 0; )
    {
        ret = callback(i, data_[i], up);
        i = (i == ret)? nodes_[i].next_: ret;
    }
}

template<typename T>
void linked_list_t<T>::walk_list( int max_walk, linked_list_flag_t flag, void* up, walk_list_callback callback )
{
    int ret;
    int idx = flag.head_;
    for (int i = 0; idx >= 0 && i < max_walk; ++i)
    {
        ret = callback(idx, data_[idx], up);
        idx = (idx == ret)? nodes_[idx].next_: ret;
    }
}

template<typename T>
void linked_list_t<T>::walk_list( linked_list_flag_t flag, void* up, walk_list_const_callback callback ) const
{
    int ret;
    for (int i = flag.head_; i >= 0; )
    {
        ret = callback(i, data_[i], up);
        i = (i == ret)? nodes_[i].next_: ret;
    }
}

template<typename T>
void linked_list_t<T>::init_list()
{
    flag_.head_ = 0;
    flag_.tail_ = list_length_ - 1;
    flag_.num_ = list_length_;
    for (int i = 0; i < list_length_; ++i)
    {
        nodes_[i].prev_ = i - 1;
        nodes_[i].next_ = i + 1;
    }

    nodes_[flag_.tail_].next_ = -1;
}

template<typename T>
void linked_list_t<T>::walk_list( void* up, walk_list_callback callback )
{
    for (int i = 0; i < list_length_; ++i)
    {
        // simply ignore return of callback
        callback(i, data_[i], up);
    }
}

template<typename T>
const T* linked_list_t<T>::get( int idx ) const
{
    if (!is_valid_index(idx))
    {
        return NULL;
    }

    return &data_[idx];
}

template<typename T>
T* linked_list_t<T>::get( int idx )
{
    if (!is_valid_index(idx))
    {
        return NULL;
    }

    return &data_[idx];
}

template<typename T>
int linked_list_t<T>::get_idx( const T* data ) const
{
    int idx = ((size_t)data - (size_t)data_) / sizeof(T);
    if (is_valid_index(idx) && &data_[idx] == data)
    {
        return idx;
    }

    return -1;
}

template<typename T>
int linked_list_t<T>::get_prev( const T* data ) const
{
    int idx = get_idx(data);
    if (is_valid_index(idx))
    {
        return nodes_[idx].prev_;
    }

    return -1;
}

template<typename T>
int linked_list_t<T>::get_next( const T* data ) const
{
    int idx = get_idx(data);
    if (is_valid_index(idx))
    {
        return nodes_[idx].next_;
    }

    return -1;
}

template<typename T>
void linked_list_t<T>::reset()
{
    init_list();
}

template<typename T>
int linked_list_t<T>::move_after( linked_list_flag_t& flag, int src, int dst )
{
    if (src == dst) return 0;

    if (!is_valid_index(src) || !is_valid_index(dst))
    {
        return -1;
    }

    int sp = nodes_[src].prev_;
    int sn = nodes_[src].next_;
    
    if (sp == dst) return 0;

    if (is_valid_index(sp)) nodes_[sp].next_ = sn;
    if (is_valid_index(sn)) nodes_[sn].prev_ = sp;

    nodes_[src].prev_ = dst;
    nodes_[src].next_ = nodes_[dst].next_;
    nodes_[dst].next_ = src;

    if (src == flag.head_)
    {
        flag.head_ = sn;
    }

    if (src == flag.tail_)
    {
        flag.tail_ = sp;
    }
    else if (dst == flag.tail_)
    {
        flag.tail_ = src;
    }

    return 0;
}

template<typename T>
int linked_list_t<T>::move_before( linked_list_flag_t& flag, int src, int dst )
{
    if (src == dst) return 0;

    if (!is_valid_index(src) || !is_valid_index(dst))
    {
        return -1;
    }

    int sp = nodes_[src].prev_;
    int sn = nodes_[src].next_;

    if (sn == dst) return 0;

    if (is_valid_index(sp)) nodes_[sp].next_ = sn;
    if (is_valid_index(sn)) nodes_[sn].prev_ = sp;

    nodes_[src].prev_ = nodes_[dst].prev_;
    nodes_[src].next_ = dst;
    nodes_[dst].prev_ = src;

    if (src == flag.head_)
    {
        flag.head_ = sn;
    }
    else if (dst == flag.head_)
    {
        flag.head_ = src;
    }

    if (src == flag.tail_)
    {
        flag.tail_ = sp;
    }

    return 0;
}

#endif
