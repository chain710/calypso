#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <tr1/functional>
#include <sys/time.h>
#include <unistd.h>
#include <inttypes.h>
#include "timer_engine.h"
#include "gtest/gtest.h"

using namespace std;
using namespace std::tr1;

char g_test_str[128];

void timer_test_callback(int64_t v)
{
    char tmp[64];
    snprintf(tmp, sizeof(tmp), "%"PRId64, v);
    strcat(g_test_str, tmp);
}

TEST(timer_test, walk_test) 
{
    timer_engine_t engine;
    int ret = engine.init(2);
    ASSERT_TRUE(ret == 0);

    // add first timer, envoke after 1 sec (loop)
    timeval nowtime;
    gettimeofday(&nowtime, NULL);
    timer_engine_t::timer_t t;
    t.deadline_ = nowtime;
    t.deadline_.tv_sec += 1;
    t.flag_ = timer_engine_t::permenant_timer;
    t.interval_ = 1000;
    t.user_ = 1;
    ret = engine.add_timer(t);
    EXPECT_EQ(ret, 0);

    // add second timer, envoke after 3 sec
    t.deadline_ = nowtime;
    t.deadline_.tv_sec += 3;
    t.flag_ = 0;
    t.interval_ = 3000;
    t.user_ = 2;
    ret = engine.add_timer(t);
    EXPECT_EQ(ret, 0);

    // expect error
    ret = engine.add_timer(t);
    EXPECT_EQ(ret, -1);

    // now run
    g_test_str[0] = '\0';
    ret = engine.walk(timer_test_callback);
    // expect null
    EXPECT_EQ(ret, 1);
    EXPECT_STREQ(g_test_str, "");

    // sleep 1 sec
    usleep(1000000);
    // expect 1
    g_test_str[0] = '\0';
    ret = engine.walk(timer_test_callback);
    EXPECT_EQ(ret, 2);
    EXPECT_STREQ(g_test_str, "1");

    // sleep 2 secs
    usleep(2000000);
    // expect 1,2
    g_test_str[0] = '\0';
    ret = engine.walk(timer_test_callback);
    EXPECT_EQ(ret, 3);
    EXPECT_STREQ(g_test_str, "12");

    // add another timer
    gettimeofday(&nowtime, NULL);
    t.deadline_ = nowtime;
    t.deadline_.tv_sec += 3;
    t.flag_ = 0;
    t.interval_ = 3000;
    t.user_ = 3;
    ret = engine.add_timer(t);
    EXPECT_EQ(ret, 0);

    // sleep 9 sec
    // expect 1
    usleep(2000000);
    g_test_str[0] = '\0';
    ret = engine.walk(timer_test_callback);
    EXPECT_EQ(ret, 2);
    EXPECT_STREQ(g_test_str, "1");
}
