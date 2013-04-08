#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <functional>
#include "linked_list.h"
#include "gtest/gtest.h"

using namespace std;
using namespace std::tr1;

class test_obj_t
{
public:
    test_obj_t() {}
    ~test_obj_t() {}
    void set(int a) {a_ = a;}

    int a_;
    char padding_[12];
};

int set_test_obj(int idx, test_obj_t& node, void* up)
{
    node.set(idx);
    return idx;
}

class list_test_fixture: public ::testing::Test
{
public:
    void SetUp()
    {
        const int list_size = 3;
        objlist_ = new linked_list_t<test_obj_t>(list_size);
        empty_list_flag(used_);

        objlist_->walk_list(objlist_->get_flag(), NULL, set_test_obj);
        for (int i = 0; i < list_size; ++i)
        {
            objlist_->append(used_);
        }
    }

    void TearDown()
    {
        delete objlist_;
    }

    linked_list_t<test_obj_t>* objlist_;
    linked_list_flag_t used_;
};

class freelist_fixture: public ::testing::Test
{
public:
    void SetUp()
    {
        const int list_size = 3;
        objlist_ = new linked_list_t<test_obj_t>(list_size);
        objlist_->walk_list(objlist_->get_flag(), NULL, set_test_obj);
    }

    void TearDown()
    {
        delete objlist_;
    }

    linked_list_t<test_obj_t>* objlist_;
};

TEST_F(list_test_fixture, swap_test) 
{
    objlist_->swap(used_, 0, 1);
    objlist_->swap(used_, 1, 2);
    objlist_->swap(used_, 1, 0);

    test_obj_t* c = objlist_->get(0);
    ASSERT_TRUE(c != NULL);
    EXPECT_EQ(objlist_->get_next(c), -1);
    EXPECT_EQ(objlist_->get_prev(c), 1);

    c = objlist_->get(1);
    ASSERT_TRUE(c != NULL);
    EXPECT_EQ(objlist_->get_next(c), 0);
    EXPECT_EQ(objlist_->get_prev(c), 2);

    c = objlist_->get(2);
    ASSERT_TRUE(c != NULL);
    EXPECT_EQ(objlist_->get_next(c), 1);
    EXPECT_EQ(objlist_->get_prev(c), -1);
}

TEST_F(list_test_fixture, remove_test) 
{
    int ret;
    for (int i = 0; i < objlist_->get_length(); ++i)
    {
        ret = objlist_->remove(used_);
        EXPECT_EQ(ret, 0);
        EXPECT_EQ(used_.num_, objlist_->get_length() - i - 1);
    }

    EXPECT_EQ(used_.head_, -1);
    EXPECT_EQ(used_.tail_, -1);
    EXPECT_EQ(objlist_->get_flag().num_, objlist_->get_length());

    ret = objlist_->remove(used_);
    EXPECT_EQ(ret, -1);
}

TEST_F(list_test_fixture, getidx_test) 
{
    int ret;
    for (int i = 0; i < objlist_->get_length(); ++i)
    {
        test_obj_t* obj = objlist_->get(i);
        ASSERT_TRUE(obj != NULL);
        ret = objlist_->get_idx(obj);
        EXPECT_EQ(ret, i);
    }
}

TEST_F(list_test_fixture, move_test) 
{
    int ret;
    ret = objlist_->move(used_, used_);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(used_.head_, 1);
    EXPECT_EQ(used_.tail_, 0);
    EXPECT_EQ(used_.num_, objlist_->get_length());

    ret = objlist_->remove(used_);
    EXPECT_EQ(ret, 0);
    ret = objlist_->remove(used_);
    EXPECT_EQ(ret, 0);
    ret = objlist_->move(used_, used_);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(used_.head_, 0);
    EXPECT_EQ(used_.tail_, 0);
    EXPECT_EQ(used_.num_, 1);
}

TEST_F(list_test_fixture, move_before) 
{
    int ret;
    int next;
    ret = objlist_->move_before(used_, 0, 0);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(used_.head_, 0);

    ret = objlist_->move_before(used_, 0, 1);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(used_.head_, 0);
    next = objlist_->get_next(objlist_->get(0));
    EXPECT_EQ(next, 1);

    ret = objlist_->move_before(used_, 1, 0);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(used_.head_, 1);
    next = objlist_->get_next(objlist_->get(1));
    EXPECT_EQ(next, 0);
}

TEST_F(list_test_fixture, move_after) 
{
    int ret;
    int next;
    ret = objlist_->move_after(used_, 0, 0);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(used_.head_, 0);

    ret = objlist_->move_after(used_, 1, 0);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(used_.head_, 0);
    next = objlist_->get_next(objlist_->get(0));
    EXPECT_EQ(next, 1);

    ret = objlist_->move_after(used_, 0, 1);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(used_.head_, 1);
    next = objlist_->get_next(objlist_->get(1));
    EXPECT_EQ(next, 0);


}
/*
TEST_F(freelist_fixture, dual_list) 
{
    int ret;
    ret = objlist_->move(used_, used_);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(used_.head_, 1);
    EXPECT_EQ(used_.tail_, 0);
    EXPECT_EQ(used_.num_, objlist_->get_length());

    ret = objlist_->remove(used_);
    EXPECT_EQ(ret, 0);
    ret = objlist_->remove(used_);
    EXPECT_EQ(ret, 0);
    ret = objlist_->move(used_, used_);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(used_.head_, 0);
    EXPECT_EQ(used_.tail_, 0);
    EXPECT_EQ(used_.num_, 1);
}
*/