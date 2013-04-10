#ifndef _HASH_MAP_H_
#define _HASH_MAP_H_

#include "linked_list.h"

// _K=keytype, _V=valuetype, _HF=hash function
template<typename _K, typename _V, typename _HF>
class hash_map_t
{
public:
    struct hash_map_node_t
    {
        _K key_;
        _V val_;
        int next_collision_;
    };

    enum hash_errno_t
    {
        no_error = 0,
        dup_key = 1,
        out_of_memory = 2,
        internal_err = 3,
        no_key = 4,
    };

    hash_map_t();
    virtual ~hash_map_t();

    int initialize(int capacity, int bucket_num);
    _V* find(const _K& key);
    int insert(const _K& key, const _V& val);
    int replace(const _K& key, const _V& val);
    int remove(const _K& key);
    void empty();
    bool is_empty() const { return hash_nodes_->get_num(lused) <= 0; }
    int get_used_num() const { return hash_nodes_->get_num(lused); }
    int get_free_num() const { return hash_nodes_->get_length() - get_used_num(); }
    int get_collision_num() const;
    int get_errno() const { return errno_; }
private:
    void init();
    // deny copy-cons
    hash_map_t(const hash_map_t& c) {}
    void reset_errno() { errno_ = no_error; }
    int put(const _K& key, const _V& val, bool ins);

    enum list_id_t
    {
        lfree = 0,
        lused = 1,
    };

    linked_list_t<hash_map_node_t>* hash_nodes_;
    int* buckets_;  // collision head
    int bucket_num_;
    int errno_;
};

template<typename _K, typename _V, typename _HF>
hash_map_t<_K, _V, _HF>::hash_map_t()
{
    init();
}

template<typename _K, typename _V, typename _HF>
hash_map_t<_K, _V, _HF>::~hash_map_t()
{
    if (buckets_) delete []buckets_;
    if (hash_nodes_) delete hash_nodes_;
    init();
}

template<typename _K, typename _V, typename _HF>
int hash_map_t<_K, _V, _HF>::initialize( int capacity, int bucket_num )
{
    reset_errno();
    bucket_num_ = bucket_num;
    hash_nodes_ = new linked_list_t<hash_map_node_t>(capacity, lused);
    buckets_ = new int[bucket_num_];

    if (NULL == hash_nodes_ || NULL == buckets_)
    {
        errno_ = internal_err;
        return -1;
    }

    for (int i = 0; i < bucket_num_; ++i)
    {
        buckets_[i] = -1;
    }

    return 0;
}

template<typename _K, typename _V, typename _HF>
_V* hash_map_t<_K, _V, _HF>::find( const _K& key )
{
    reset_errno();
    int node_id = buckets_[_HF()(key) % bucket_num_];
    hash_map_node_t* hnode = hash_nodes_->get(node_id);

    // search collision list
    while (hnode && key != hnode->key_)
    {
        hnode = hash_nodes_->get(hnode->next_collision_);
    }

    return hnode? &hnode->val_: NULL;
}

template<typename _K, typename _V, typename _HF>
int hash_map_t<_K, _V, _HF>::insert( const _K& key, const _V& val )
{
    return put(key, val, true);
}

template<typename _K, typename _V, typename _HF>
int hash_map_t<_K, _V, _HF>::replace( const _K& key, const _V& val )
{
    return put(key, val, false);
}

template<typename _K, typename _V, typename _HF>
int hash_map_t<_K, _V, _HF>::put( const _K& key, const _V& val, bool ins )
{
    reset_errno();
    int idx = _HF()(key) % bucket_num_;
    int node_id = buckets_[idx];
    hash_map_node_t* hnode = hash_nodes_->get(node_id);

    // search collision list
    while (hnode && key != hnode->key_)
    {
        hnode = hash_nodes_->get(hnode->next_collision_);
    }

    if (hnode)
    {
        // find dup
        if (ins)
        {
            errno_ = dup_key;
            return -1;
        }
        else
        {
            hnode->val_ = val;
            return 0;
        }
    }

    // alloc one node
    int ret = hash_nodes_->append(lused);
    if (ret < 0)
    {
        errno_ = out_of_memory;
        return -1;
    }

    int alloc_idx = hash_nodes_->get_tail(lused);
    hash_map_node_t* ins_node = hash_nodes_->get(alloc_idx);
    ins_node->key_ = key;
    ins_node->val_ = val;
    ins_node->next_collision_ = buckets_[idx];
    buckets_[idx] = alloc_idx;
    return 0;
}

template<typename _K, typename _V, typename _HF>
int hash_map_t<_K, _V, _HF>::remove( const _K& key )
{
    reset_errno();
    int idx = _HF()(key) % bucket_num_;
    int node_id = buckets_[idx];
    hash_map_node_t* hnode = hash_nodes_->get(node_id);
    hash_map_node_t* hprev = NULL;
    // search collision list
    while (hnode && key != hnode->key_)
    {
        hprev = hnode;
        hnode = hash_nodes_->get(hnode->next_collision_);
    }

    if (NULL == hnode)
    {
        errno_ = no_key;
        return -1;
    }

    if (hprev)
    {
        hprev->next_collision_ = hnode->next_collision_;
    }
    else
    {
        buckets_[idx] = hnode->next_collision_;
    }

    int del_id = hash_nodes_->get_idx(hnode);
    int ret = hash_nodes_->swap(hash_nodes_->get_head(lused), del_id);
    if (ret < 0)
    {
        errno_ = internal_err;
        return -1;
    }

    ret = hash_nodes_->remove(lused);
    if (ret < 0)
    {
        errno_ = internal_err;
        return -1;
    }

    return 0;
}

template<typename _K, typename _V, typename _HF>
void hash_map_t<_K, _V, _HF>::empty()
{
    for (int i = 0; i < bucket_num_; ++i)
    {
        buckets_[i] = -1;
    }

    hash_nodes_->reset();
}

template<typename _K, typename _V, typename _HF>
void hash_map_t<_K, _V, _HF>::init()
{
    reset_errno();
    buckets_ = NULL;
    hash_nodes_ = NULL;
    bucket_num_ = 0;
}

template<typename _K, typename _V, typename _HF>
int _add_collision_num(int idx, const typename hash_map_t<_K, _V, _HF>::hash_map_node_t& node, void* up)
{
    if (node.next_collision_ >= 0 && up)
    {
        int& num = *(int*)up;
        ++num;
    }

    return idx;
}

template<typename _K, typename _V, typename _HF>
int hash_map_t<_K, _V, _HF>::get_collision_num() const
{
    int num = 0;
    hash_nodes_->walk_list(lused, &num, _add_collision_num<_K, _V, _HF>);
    return num;
}

#endif
