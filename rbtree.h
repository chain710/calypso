#ifndef _RBTREE_H_
#define _RBTREE_H_

#include "linked_list.h"
#include <string.h>

enum rbtree_color_t 
{
    rb_red = 0,
    rb_black = 1,
};

template<typename _K, typename _V, typename _CMP>
class llrbtree_t
{
private:
    enum list_id_t
    {
        lfree = 0,
        lused = 1,
    };
public:
    struct rbtree_node_t
    {
        rbtree_node_t()
        {
            left_ = NULL;
            right_ = NULL;
            size_ = 1;
            color_ = rb_red;
        }

        rbtree_node_t* left_;
        rbtree_node_t* right_;
        int size_;
        int color_;
        _K key_;
        _V val_;
    };

    llrbtree_t()
    {
        root_ = NULL;
        nodes_ = NULL;
    }

    virtual ~llrbtree_t()
    {
        finalize();
    }

    int initialize(int capacity)
    {
        nodes_ = new linked_list_t<rbtree_node_t>(capacity, lused);
        if (NULL == nodes_)
        {
            return -1;
        }

        return 0;
    }

    void finalize()
    {
        if (nodes_)
        {
            delete nodes_;
            nodes_ = NULL;
        }

        root_ = NULL;
    }

    const rbtree_node_t* get_root() const { return root_; }

    rbtree_node_t* get_root() { return root_; }

    int insert(const _K& k, const _V& v)
    {
        int ret = nodes_->append(lused);
        if (ret < 0)
        {
            // out of nodes
            return -1;
        }

        rbtree_node_t* node = nodes_->get(nodes_->get_tail(lused));
        node->key_ = k;
        node->val_ = v;

        root_ = insert(root_, *node);
        root_->color_ = rb_black;
        return 0;
    }

    int remove(const _K& key)
    {
        if (NULL == root_)
        {
            return -1;
        }

        rbtree_node_t* n = get_by_key(root_, key);
        if (NULL == n)
        {
            return -1;
        }

        int swp_idx = nodes_->get_idx(n);

        if (!is_red(root_->left_) && !is_red(root_->right_))
        {
            root_->color_ = rb_red;
        }

        root_ = remove(*root_, key);
        if (root_)
        {
            root_->color_ = rb_black;
        }

        int ret = nodes_->swap(nodes_->get_head(lused), swp_idx);
        if (ret < 0)
        {
            return -1;
        }

        nodes_->remove(lused);
        return 0;
    }

    _V* get_by_key(const _K& key)
    {
        rbtree_node_t* n = get_by_key(root_, key);
        return n? &n->val_: NULL;
    }

    _V* get_ceiling(const _K& key)
    {
        rbtree_node_t* n = get_ceiling(root_, key);
        return n? &n->val_: NULL;
    }

    _V* get_floor(const _K& key)
    {
        rbtree_node_t* n = get_floor(root_, key);
        return n? &n->val_: NULL;
    }

    _V* get_by_rank(int rank)
    {
        rbtree_node_t* n = get_by_rank(root_, rank);
        return n? &n->val_: NULL;
    }

    int get_rank(const _K& key) const { return get_rank(root_, key); }

    _K* get_min_key()
    {
        if (NULL == root_) return NULL;
        rbtree_node_t& m = get_min(*root_);
        return &m.key_;
    }
private:
    // deny copy-cons
    llrbtree_t(const llrbtree_t& c) {}
    // ��ɫ
    void flip(rbtree_node_t* root)
    {
        if (NULL == root) return;
        root->color_ = (rb_red == root->color_)? rb_black: rb_red;
    }

    // ��root������node��������root
    rbtree_node_t* insert(rbtree_node_t* root, rbtree_node_t& node)
    {
        if (NULL == root)
        {
            node.color_ = rb_red;
            node.size_ = 1;
            return &node;
        }

        int cmp = _CMP()(node.key_, root->key_);
        if (cmp < 0)
        {
            root->left_ = insert(root->left_, node);
        }
        else if (cmp > 0)
        {
            root->right_ = insert(root->right_, node);
        }
        else
        {
            // dup key, do nothing
            return root;
        }

        // ���������Ͷ�ڵ�����
        if (is_red(root->right_) && !is_red(root->left_))
        {
            root = rotate_left(*root);
        }

        if (is_red(root->left_) && is_red(root->left_->left_))
        {
            root = rotate_right(*root);
        }

        if (is_red(root->left_) && is_red(root->right_))
        {
            flip_color(*root);
        }

        update_size(*root);
        return root;
    }

    // ����root����������root
    rbtree_node_t* rotate_left(rbtree_node_t& root)
    {
        rbtree_node_t* rchild = root.right_;
        if (NULL == rchild || !is_red(rchild))
        {
            // error
            return NULL;
        }

        root.right_ = rchild->left_;
        rchild->left_ = &root;
        rchild->color_ = root.color_;
        root.color_ = rb_red;
        rchild->size_ = root.size_;
        update_size(root);
        return rchild;
    }

    // ����root����������root
    rbtree_node_t* rotate_right(rbtree_node_t& root)
    {
        // lchild must be red and cant be null
        rbtree_node_t* lchild = root.left_;
        if (NULL == lchild || !is_red(lchild))
        {
            //error
            return NULL;
        }

        root.left_ = lchild->right_;
        lchild->right_ = &root;
        lchild->color_ = root.color_;
        root.color_ = rb_red;
        lchild->size_ = root.size_;
        update_size(root);
        return lchild;
    }

    // ����ɫ�ڵ��·ţ���֤�������к�ɫ�ڵ�
    rbtree_node_t* move_red_left(rbtree_node_t& root)
    {
        // ����ɫ�ڵ�root�·ŵ����������γ�4-node
        if (!is_red(&root) || is_red(root.left_) || is_red(root.left_->left_))
        {
            // error
            return NULL;
        }

        flip_color(root);
        if (is_red(root.right_->left_))
        {
            // ����γ���5-node,�黹һ���ڵ���ϲ㣬������ѳ�һ��2-node��һ��3-node
            root.right_ = rotate_right(*root.right_);
            rbtree_node_t* n = rotate_left(root);
            flip_color(*n);
            return n;
        }

        return &root;
    }

    // ����ɫ�ڵ��·ţ���֤�������к�ɫ�ڵ�
    rbtree_node_t* move_red_right(rbtree_node_t& root)
    {
        // �Ѻ�ɫ�ڵ��Ƶ�������
        // ����������ǰ����������, root�Ǻ�ɫ, root.right��black, root.right.left��black
        if (!is_red(&root) || is_red(root.right_) || is_red(root.right_->left_))
        {
            // error
            return NULL;
        }

        flip_color(root);
        if (is_red(root.left_->left_))
        {
            rbtree_node_t *n = rotate_right(root);
            flip_color(*n);
            return n;
        }

        return &root;
    }

    // ��ɫ
    void flip_color(rbtree_node_t& root)
    {
        flip(&root);
        flip(root.left_);
        flip(root.right_);
    }

    // ɾ����������root
    rbtree_node_t* remove(rbtree_node_t& root, const _K& key)
    {
        // �ݹ��б�֤root isred || root.left isred
        rbtree_node_t* n = &root;
        if (_CMP()(key, n->key_) < 0)
        {
            // ��������ɾ�������l��ll����black��������
            if (n->left_)
            {
                if (!is_red(n->left_) && !is_red(n->left_->left_))
                {
                    n = move_red_left(*n);
                }

                // left cant be null
                n->left_ = remove(*n->left_, key);
            }
            // else �Ҳ���������ԭʼ���ڵ�
        }
        else
        {
            if (is_red(n->left_))
            {
                // ��ʾroot �� root.right��black������������
                n = rotate_right(*n);
            }

            if (0 == _CMP()(key, n->key_) && NULL == n->right_)
            {
                // �ҵ��ˣ���ʱ��right-lean rb��h��red����h.right��red�������ݺ�������ʿ����ƶϣ����right��null��left�϶�Ҳ��null(h��red)
                return NULL;
            }

            if (n->right_)
            {
                // ��֤�������ĸ�������һ����red��������������
                if (!is_red(n->right_) && !is_red(n->right_->left_))
                {
                    // ��ʱ���������Ҷ�����black, ��root=red root.left=black root.right=black root.right.left=black
                    n = move_red_right(*n);
                }

                if (0 == _CMP()(key, n->key_))
                {
                    // n�滻Ϊ��������Сֵ
                    rbtree_node_t &rmin = get_min(*n->right_);
                    n->right_ = remove_min(*n->right_);

                    rmin.left_ = n->left_;
                    rmin.right_ = n->right_;
                    rmin.color_ = n->color_;
                    update_size(rmin);
                    n = &rmin;
                }
                else
                {
                    n->right_ = remove(*n->right_, key);
                }
            }
        }

        return balance(*n);
    }

    // ɾ����Сֵ, remove_min�����б�֤root����root->left_��red
    rbtree_node_t* remove_min(rbtree_node_t& root)
    {
        if (NULL == root.left_)
        {
            // ˵��root��red��ֱ��ɾ��
            return NULL;
        }

        rbtree_node_t* n = &root;
        if (!is_red(root.left_) && !is_red(root.left_->left_))
        {
            // ˵��root��red��������������
            n = move_red_left(*n);
        }

        n->left_ = remove_min(*n->left_);
        return balance(*n);
    }

    // ����ƽ��
    rbtree_node_t* balance(rbtree_node_t& root)
    {
        rbtree_node_t* n = &root;
        if (is_red(n->right_))
        {
            n = rotate_left(*n);
        }

        if (is_red(n->left_) && is_red(n->left_->left_))
        {
            n = rotate_right(*n);
        }

        if (is_red(n->left_) && is_red(n->right_))
        {
            flip_color(*n);
        }

        update_size(*n);
        return n;
    }

    // �Ƿ��ɫ
    bool is_red(const rbtree_node_t* node) const { return node && node->color_ == rb_red; }
    // ������Сkey�ڵ�
    rbtree_node_t& get_min(rbtree_node_t& root)
    {
        return root.left_? get_min(*root.left_): root;
    }

    // �ݹ鰴��������
    rbtree_node_t* get_by_rank(const rbtree_node_t* root, int rank)
    {
        if (NULL == root || rank <= 0 || rank > root->size_)
        {
            return NULL;
        }

        int lsize = get_size(root->left_);
        if (rank == lsize + 1)
        {
            return root;
        }
        else if (rank < lsize)
        {
            return get_by_rank(root->left_, rank);
        }

        return get_by_rank(root->right_, rank - lsize - 1);
    }

    // �ݹ��ȡ����, ���key�����ڷ���-1
    int get_rank(const rbtree_node_t* root, const _K& key)
    {
        if (NULL == root)
        {
            return -1;
        }

        int cmp = _CMP()(key, root->key_);
        int lsize = get_size(root->left_);
        if (0 == cmp)
        {
            return lsize + 1;
        }
        else if (cmp < 0)
        {
            return get_rank(root->left_, key);
        }
        else
        {
            int rrank = get_rank(root->right_, key);
            return rrank < 0? -1: (lsize + 1 + rrank);
        }
    }

    // ��ȡ�ڵ�size
    int get_size(const rbtree_node_t* root) const { return root? root->size_: 0; }
    // ����size
    void update_size(rbtree_node_t& root) { root.size_ = 1 + get_size(root.left_) + get_size(root.right_); }
    // �ݹ鰴key����
    rbtree_node_t* get_by_key(rbtree_node_t* root, const _K& key)
    {
        if (NULL == root)
        {
            return NULL;
        }

        int cmp = _CMP()(key, root->key_);
        if (cmp < 0)
        {
            return get_by_key(root->left_, key);
        }
        else if (cmp > 0)
        {
            return get_by_key(root->right_, key);
        }

        // found
        return root;
    }

    // �ݹ�����>=key����С�ڵ�
    rbtree_node_t* get_ceiling(rbtree_node_t* root, const _K& key)
    {
        if (NULL == root)
        {
            return NULL;
        }

        rbtree_node_t* n = root;
        int cmp = _CMP()(key, root->key_);
        if (cmp > 0)
        {
            n = get_ceiling(root->right_, key);
            n = n? n: root;
        }
        else if (cmp < 0)
        {
            n = get_ceiling(root->left_, key);
            n = n? n: root;
        }

        return _CMP()(key, n->key_) <= 0? n: NULL;
    }

    // �ݹ�����<=key�����ڵ�
    rbtree_node_t* get_floor(rbtree_node_t* root, const _K& key)
    {
        if (NULL == root)
        {
            return NULL;
        }

        rbtree_node_t* n = root;
        int cmp = _CMP()(key, root->key_);
        if (cmp > 0)
        {
            n = get_floor(root->right_, key);
            n = n? n: root;
        }
        else if (cmp < 0)
        {
            n = get_floor(root->left_, key);
            n = n? n: root;
        }

        return _CMP()(key, n->key_) >= 0? n: NULL;
    }

    // ���ڵ�
    rbtree_node_t* root_;
    linked_list_t<rbtree_node_t>* nodes_;
};

#endif
