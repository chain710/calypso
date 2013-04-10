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
        int lid_;
        int prev_;
        int next_;
    };

    typedef std::tr1::function<int (int idx, T& node, void*)> walk_list_callback;
    typedef std::tr1::function<int (int idx, const T& node, void*)> walk_list_const_callback;

    // 内部保留一个list(0)记录尚未分配的，故实际listnum是max_list+1
    linked_list_t(int max_length, int max_list);
    virtual ~linked_list_t();
    
    // 交换两个元素在链表中的顺序
    int swap(int left, int right);
    // move node from flag(head) to list(tail)
    int remove(int lid);
    // move node from list(head) to flag(tail)
    int append(int lid);
    // move node from src.head to dst.tail
    int move(int src_lid, int dst_lid);
    // move src(flag) after dst
    int move_after(int src, int dst);
    // move src(flag) before dst
    int move_before( int src, int dst );

    void walk_list(int lid, void* up, walk_list_callback callback);
    void walk_list(int lid, void* up, walk_list_const_callback callback) const;
    void walk_list(void* up, walk_list_callback callback);
    void walk_list(int max_walk, int lid, void* up, walk_list_callback callback);
    T* get(int idx);
    const T* get(int idx) const;
    int get_idx(const T* data) const;
    int get_next(const T* data) const;
    int get_prev(const T* data) const;
    void reset();
    int get_length() const { return list_length_; }
    inline int get_num(int lid) const
    {
        if (!is_valid_list(lid)) return -1;
        return lflag_[lid].num_;
    }

    inline int get_head(int lid) const
    {
        if (!is_valid_list(lid)) return -1;
        return lflag_[lid].head_;
    }

    inline int get_tail(int lid) const
    {
        if (!is_valid_list(lid)) return -1;
        return lflag_[lid].tail_;
    }

    inline int get_list_id(int idx) const
    {
        if (!is_valid_index(idx)) return -1;
        return nodes_[idx].lid_;
    }
private:
    linked_list_t(const linked_list_t& l){}
    void init_list();
    void init_node(list_node_t& n) { n.prev_ = -1; n.next_ = -1; }
    inline bool is_valid_index(int i) const { return i >= 0 && i < list_length_; }
    inline bool is_valid_list(int i) const { return i >= 0 && i <= max_list_; }
    linked_list_flag_t* lflag_;
    list_node_t* nodes_;
    T* data_;
    int list_length_;
    int max_list_;
};

template<typename T>
linked_list_t<T>::linked_list_t( int max_length, int max_list )
{
    nodes_ = new list_node_t[max_length];
    data_ = new T[max_length];
    lflag_ = new linked_list_flag_t[max_list+1];
    list_length_ = max_length;
    max_list_ = max_list;
    init_list();
}

template<typename T>
linked_list_t<T>::~linked_list_t()
{
    if (lflag_)
    {
        delete []lflag_;
        lflag_ = NULL;
    }

    if (nodes_)
    {
        delete []nodes_;
        nodes_ = NULL;
    }

    if (data_)
    {
        delete []data_;
        data_ = NULL;
    }
    
    list_length_ = 0;
}

template<typename T>
int linked_list_t<T>::swap( int left, int right )
{
    if (!is_valid_index(left) || !is_valid_index(right)
        || nodes_[left].lid_ != nodes_[right].lid_)
    {
        return -1;
    }

    if (right == left) return 0;
    linked_list_flag_t& flag = lflag_[nodes_[left].lid_];

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
int linked_list_t<T>::remove( int lid )
{
    return move(lid, 0);
}

template<typename T>
int linked_list_t<T>::append( int lid )
{
    return move(0, lid);
}

template<typename T>
int linked_list_t<T>::move( int src_lid, int dst_lid )
{
    if (!is_valid_list(src_lid) || !is_valid_list(dst_lid))
    {
        return -1;
    }

    linked_list_flag_t& src = lflag_[src_lid];
    linked_list_flag_t& dst = lflag_[dst_lid];
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
    mnode.lid_ = dst_lid;
    dst.tail_ = midx;
    ++dst.num_;
    return 0;
}

template<typename T>
void linked_list_t<T>::walk_list( int lid, void* up, walk_list_callback callback )
{
    if (!is_valid_list(lid)) return;
    int ret;
    for (int i = lflag_[lid].head_; i >= 0; )
    {
        ret = callback(i, data_[i], up);
        i = (i == ret)? nodes_[i].next_: ret;
    }
}

template<typename T>
void linked_list_t<T>::walk_list( int lid, void* up, walk_list_const_callback callback ) const
{
    if (!is_valid_list(lid)) return;
    int ret;
    for (int i = lflag_[lid].head_; i >= 0; )
    {
        ret = callback(i, data_[i], up);
        i = (i == ret)? nodes_[i].next_: ret;
    }
}

template<typename T>
void linked_list_t<T>::walk_list( int max_walk, int lid, void* up, walk_list_callback callback )
{
    if (!is_valid_list(lid)) return;
    int ret;
    int idx = lflag_[lid].head_;
    for (int i = 0; idx >= 0 && i < max_walk; ++i)
    {
        ret = callback(idx, data_[idx], up);
        idx = (idx == ret)? nodes_[idx].next_: ret;
    }
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
void linked_list_t<T>::init_list()
{
    for (int i = 0; i <= max_list_; ++i)
    {
        empty_list_flag(lflag_[i]);
    }

    // all belong to listflag[0]
    lflag_[0].head_ = 0;
    lflag_[0].tail_ = list_length_ - 1;
    lflag_[0].num_ = list_length_;
    for (int i = 0; i < list_length_; ++i)
    {
        nodes_[i].lid_ = 0;
        nodes_[i].prev_ = i - 1;
        nodes_[i].next_ = i + 1;
    }

    nodes_[list_length_ - 1].next_ = -1;
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
int linked_list_t<T>::move_after( int src, int dst )
{
    if (!is_valid_index(src) || !is_valid_index(dst)
        || nodes_[src].lid_ != nodes_[dst].lid_)
    {
        return -1;
    }

    linked_list_flag_t& flag = lflag_[nodes_[src].lid_];
    if (src == dst) return 0;
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
int linked_list_t<T>::move_before( int src, int dst )
{
    if (!is_valid_index(src) || !is_valid_index(dst)
        || nodes_[src].lid_ != nodes_[dst].lid_)
    {
        return -1;
    }

    if (src == dst) return 0;
    linked_list_flag_t& flag = lflag_[nodes_[src].lid_];
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
