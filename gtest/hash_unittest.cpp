#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>
#include <stdint.h>
#include "hash_map.h"
#include "gtest/gtest.h"

using namespace std;
using namespace std::tr1;
#define FNV_32_PRIME 0x01000193
#define FNV_32_INIT 0x811c9dc5

inline uint32_t fnv1a(const char *key, int key_len)
{
    uint32_t h = FNV_32_INIT;
    int i;

    for (i=0; i<key_len; i++) {
        h ^= (uint32_t)key[i];
        h *= FNV_32_PRIME;
    }

    return h;
}

struct int_hash_functor
{
    unsigned int operator ()(const int& key)
    {
        return fnv1a((const char*)&key, sizeof(key));
    }
};

TEST(hashmap, generic) 
{
    const int hash_capacity = 37;
    typedef hash_map_t<int, std::string, int_hash_functor> int2str_hash;
    int2str_hash h;
    string *strval;
    int ret = h.initialize(hash_capacity, hash_capacity/2);
    ASSERT_TRUE(0 == ret);

    char tmp_str[128];
    for (int i = 0; i < hash_capacity; ++i)
    {
        snprintf(tmp_str, sizeof(tmp_str), "val%d", i);
        ret = h.insert(i, tmp_str);
        EXPECT_EQ(0, ret);
    }

    for (int i = 0; i < hash_capacity; ++i)
    {
        snprintf(tmp_str, sizeof(tmp_str), "val%d", i);
        strval = h.find(i);
        ASSERT_TRUE(strval != NULL);
        EXPECT_STREQ(tmp_str, strval->c_str());
    }

    EXPECT_EQ(h.get_used_num(), hash_capacity);

    ret = h.insert(-1, "fail");
    EXPECT_EQ(-1, ret);

    for (int i = 0; i < hash_capacity; ++i)
    {
        snprintf(tmp_str, sizeof(tmp_str), "val%d", i+1);
        ret = h.replace(i, tmp_str);
        EXPECT_EQ(0, ret);
    }

    for (int i = 0; i < hash_capacity; ++i)
    {
        snprintf(tmp_str, sizeof(tmp_str), "val%d", i+1);
        strval = h.find(i);
        ASSERT_TRUE(strval != NULL);
        EXPECT_STREQ(tmp_str, strval->c_str());
        ret = h.remove(i);
        EXPECT_EQ(0, ret);
    }

    ret = h.insert(-1, "-1");
    EXPECT_EQ(0, ret);
    ret = h.insert(-1, "-2");
    EXPECT_EQ(-1, ret);
}
