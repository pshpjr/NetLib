//
// Created by pshpj on 24. 8. 28.
//

#ifndef MULTITHREADOBJECTPOOLTEST_H
#define MULTITHREADOBJECTPOOLTEST_H
#include "MultiThreadObjectPool.h"

#include <gtest/gtest.h>

TEST(MultiThreadObjectPool, Init)
{
    MultiThreadObjectPool<int> pool(10);
    EXPECT_TRUE(pool.Size() == 10);
}

TEST(MultiThreadObjectPool, Alloc)
{
    MultiThreadObjectPool<int> pool(1);
    auto data = pool.Alloc();
    EXPECT_TRUE(pool.Size() == 0);
    pool.Free(data);
}

TEST(MultiThreadObjectPool, Free)
{
    MultiThreadObjectPool<int> pool(1);
    {
        auto data = pool.Alloc();
        pool.Free(data);
    }
    EXPECT_TRUE(pool.Size() == 1);
}
TEST(MultiThreadObjectPool, MemReuse)
{
    int* fstPtr = nullptr;
    MultiThreadObjectPool<int> pool(100);
    {
        fstPtr = pool.Alloc();
        pool.Free(fstPtr);
    }
    {
        auto t = pool.Alloc();
        EXPECT_TRUE(fstPtr == t);
        pool.Free(t);
    }

}

TEST(MultiThreadObjectPool, DupAlloc)
{
    void* fstPtr = nullptr;
    MultiThreadObjectPool<int> pool(1);
    {
        auto t = pool.Alloc();
        auto t2 = pool.Alloc();
        EXPECT_TRUE(pool.Size() == 0);
        pool.Free(t);
        pool.Free(t2);
    }

    EXPECT_TRUE(pool.Size() == 2);
}
#endif //MULTITHREADOBJECTPOOLTEST_H
