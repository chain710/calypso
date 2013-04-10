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
        objlist_ = new linked_list_t<test_obj_t>(list_size, 2);

        objlist_->walk_list(0, NULL, set_test_obj);
        for (int i = 0; i < list_size; ++i)
        {
            objlist_->append(1);
        }
    }

    void TearDown()
    {
        delete objlist_;
    }

    linked_list_t<test_obj_t>* objlist_;
};

TEST_F(list_test_fixture, swap_test) 
{
    objlist_->swap(0, 1);
    objlist_->swap(1, 2);
    objlist_->swap(1, 0);

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
        ret = objlist_->remove(1);
        EXPECT_EQ(ret, 0);
        EXPECT_EQ(objlist_->get_num(1), objlist_->get_length() - i - 1);
    }

    EXPECT_EQ(objlist_->get_head(1), -1);
    EXPECT_EQ(objlist_->get_tail(1), -1);
    EXPECT_EQ(objlist_->get_num(0), objlist_->get_length());

    ret = objlist_->remove(1);
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
    ret = objlist_->move(1, 1);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(objlist_->get_head(1), 1);
    EXPECT_EQ(objlist_->get_tail(1), 0);
    EXPECT_EQ(objlist_->get_num(1), objlist_->get_length());

    ret = objlist_->remove(1);
    EXPECT_EQ(ret, 0);
    ret = objlist_->remove(1);
    EXPECT_EQ(ret, 0);
    ret = objlist_->move(1, 1);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(objlist_->get_head(1), 0);
    EXPECT_EQ(objlist_->get_tail(1), 0);
    EXPECT_EQ(objlist_->get_num(1), 1);
}

TEST_F(list_test_fixture, move_before) 
{
    int ret;
    int next;
    ret = objlist_->move_before(0, 0);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(objlist_->get_head(1), 0);

    ret = objlist_->move_before(0, 1);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(objlist_->get_head(1), 0);
    next = objlist_->get_next(objlist_->get(0));
    EXPECT_EQ(next, 1);

    ret = objlist_->move_before(1, 0);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(objlist_->get_head(1), 1);
    next = objlist_->get_next(objlist_->get(1));
    EXPECT_EQ(next, 0);
}

TEST_F(list_test_fixture, move_after) 
{
    int ret;
    int next;
    ret = objlist_->move_after(0, 0);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(objlist_->get_head(1), 0);

    ret = objlist_->move_after(1, 0);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(objlist_->get_head(1), 0);
    next = objlist_->get_next(objlist_->get(0));
    EXPECT_EQ(next, 1);

    ret = objlist_->move_after(0, 1);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(objlist_->get_head(1), 1);
    next = objlist_->get_next(objlist_->get(1));
    EXPECT_EQ(next, 0);
    next = objlist_->get_next(objlist_->get(0));
    EXPECT_EQ(next, 2);

    ret = objlist_->move_after(1, 2);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(objlist_->get_tail(1), 1);
    next = objlist_->get_next(objlist_->get(1));
    EXPECT_EQ(next, -1);
}

TEST_F(list_test_fixture, dual_list) 
{
    int ret;
    ret = objlist_->move(1, 2);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(objlist_->get_head(1), 1);
    EXPECT_EQ(objlist_->get_head(2), 0);
    EXPECT_EQ(objlist_->get_num(1), 2);
    EXPECT_EQ(objlist_->get_num(2), 1);
    EXPECT_EQ(objlist_->get_list_id(0), 2);
    EXPECT_EQ(objlist_->get_list_id(1), 1);
    EXPECT_EQ(objlist_->get_list_id(2), 1);

    ret = objlist_->swap(0, 1);
    EXPECT_EQ(-1, ret);
    ret = objlist_->swap(0, 2);
    EXPECT_EQ(-1, ret);
    ret = objlist_->swap(1, 2);
    EXPECT_EQ(0, ret);

    ret = objlist_->remove(2);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(objlist_->get_num(0), 1);
    EXPECT_EQ(objlist_->get_num(1), 2);
    EXPECT_EQ(objlist_->get_num(2), 0);
}
