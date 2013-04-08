#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>
#include "ring_queue.h"
#include "gtest/gtest.h"

using namespace std;
using namespace std::tr1;

TEST(ring_queue, boundary) 
{
    const int rq_len = 21;
    ring_queue_t rq;
    int ret = rq.initialize(rq_len);
    ASSERT_TRUE(ret == 0);

    EXPECT_EQ(rq.get_free_len(), rq_len - 1);
    EXPECT_EQ(rq.get_used_len(), 0);

    string test_str("123");
    rq.produce(test_str.c_str(), test_str.length());
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(rq.get_used_len(), (int)sizeof(ring_queue_t::ring_block_t)+(int)test_str.length());

    char consume_data[rq_len];
    memset(consume_data, 0, sizeof(consume_data));
    ret = rq.consume(consume_data, sizeof(consume_data));
    EXPECT_EQ(ret, (int)test_str.length());
    EXPECT_STREQ(consume_data, test_str.c_str());

    test_str = "1234";
    ret = rq.produce(test_str.c_str(), test_str.length());
    EXPECT_EQ(ret, 0);

    test_str = "12";
    ret = rq.produce(test_str.c_str(), test_str.length());
    EXPECT_EQ(ret, 0);

    // full
    test_str = "123";
    ret = rq.produce(test_str.c_str(), test_str.length());
    EXPECT_EQ(ret, -1);

    // just fit
    test_str = "12";
    ret = rq.produce(test_str.c_str(), test_str.length());
    EXPECT_EQ(ret, 0);

    rq.clear();
    test_str = "1234";
    ret = rq.produce(test_str.c_str(), test_str.length());
    EXPECT_EQ(ret, 0);

    test_str = "12345";
    ret = rq.produce(test_str.c_str(), test_str.length());
    EXPECT_EQ(ret, 0);

    // full
    test_str = "1";
    ret = rq.produce(test_str.c_str(), test_str.length());
    EXPECT_EQ(ret, -1);

    // consume 1234
    memset(consume_data, 0, sizeof(consume_data));
    ret = rq.consume(consume_data, sizeof(consume_data));
    EXPECT_STREQ(consume_data, "1234");

    ret = rq.produce(test_str.c_str(), test_str.length());
    EXPECT_EQ(ret, 0);

    memset(consume_data, 0, sizeof(consume_data));
    ret = rq.consume(consume_data, sizeof(consume_data));
    EXPECT_STREQ(consume_data, "12345");

    memset(consume_data, 0, sizeof(consume_data));
    ret = rq.consume(consume_data, sizeof(consume_data));
    EXPECT_STREQ(consume_data, "1");

    memset(consume_data, 0, sizeof(consume_data));
    ret = rq.consume(consume_data, sizeof(consume_data));
    EXPECT_EQ(ret, 0);

    rq.clear();
    test_str = "12345";
    ret = rq.produce(test_str.c_str(), test_str.length());
    EXPECT_EQ(ret, 0);

    test_str = "12345";
    ret = rq.produce(test_str.c_str(), test_str.length());
    EXPECT_EQ(ret, 0);

    memset(consume_data, 0, sizeof(consume_data));
    ret = rq.consume(consume_data, sizeof(consume_data));
    EXPECT_STREQ(consume_data, "12345");

    test_str = "1234";
    ret = rq.produce(test_str.c_str(), test_str.length());
    EXPECT_EQ(ret, 0);

    ret = rq.skip_consume();

    memset(consume_data, 0, sizeof(consume_data));
    ret = rq.consume(consume_data, sizeof(consume_data));
    EXPECT_STREQ(consume_data, "1234");

    EXPECT_EQ(rq.get_used_len(), 0);
}
