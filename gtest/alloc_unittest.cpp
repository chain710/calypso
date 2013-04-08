#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>
#include <stdint.h>
#include "allocator.h"
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

TEST(allocator, generic) 
{
    const int max_allocator_num = 2;
    const int a_capacity = 8;
    
    int ret;
    dynamic_allocator_t da;
    ret = da.initialize(max_allocator_num);

    fixed_size_allocator_t sub_a[max_allocator_num];

    const int reserve_bytes = 8;    //sizeof(buffer_head_t)
    int base_size = 32;
    int a_size = base_size;
    for (int i = 0; i < max_allocator_num; ++i)
    {
        ret = sub_a[i].initialize(a_size + reserve_bytes, a_capacity);
        ASSERT_TRUE(0 == ret);
        a_size *= 2;
        ret = da.add_allocator(sub_a[i]);
        ASSERT_TRUE(0 == ret);
    }

    a_size/=2;
    char* buf;
    buf = da.alloc(a_size);
    EXPECT_NE(buf, (char*)NULL);

    EXPECT_EQ(NULL, da.alloc(a_size+1));

    ret = da.dealloc(buf);
    EXPECT_EQ(ret, 0);

    char* addrs[max_allocator_num*a_capacity];
    for (int i = 0; i < max_allocator_num*a_capacity; ++i )
    {
        addrs[i] = da.alloc(1);
        EXPECT_NE(addrs[i], (char*)NULL);
    }

    EXPECT_EQ(NULL, da.alloc(1));

    for (int i = 0; i < max_allocator_num*a_capacity; ++i )
    {
        EXPECT_EQ(0, da.dealloc(addrs[i]));
    }

    for (int i = 0; i < a_capacity; ++i)
    {
        addrs[i] = da.alloc(a_size);
    }

    EXPECT_EQ(NULL, da.alloc(a_size));

    for (int i = 0; i < a_capacity; ++i)
    {
        EXPECT_EQ(0, da.dealloc(addrs[i]));
    }

    for (int i = 0; i < a_capacity; ++i)
    {
        addrs[i] = da.alloc(base_size);
    }

    EXPECT_NE((char*)NULL, da.alloc(base_size));
}
