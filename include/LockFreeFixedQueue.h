#pragma once
#include <atomic>

#include "Container.h"

template <typename T, int BufferSize>
class LockFreeFixedQueue
{
    static_assert((BufferSize & BufferSize - 1) == 0);

    struct Node
    {
        std::atomic<bool> isUsed = false;
        T data = T();
    };

public:
    constexpr LockFreeFixedQueue() : indexMask(BufferSize - 1)
                                   , buffer(BufferSize)
    {
    }

    LockFreeFixedQueue(const LockFreeFixedQueue& other) = delete;
    LockFreeFixedQueue(LockFreeFixedQueue&& other) = delete;

    LockFreeFixedQueue& operator =(const LockFreeFixedQueue& other) = delete;
    LockFreeFixedQueue& operator =(LockFreeFixedQueue&& other) = delete;

    ~LockFreeFixedQueue() = default;

    int Size() const
    {
        auto head = headIndex.load() & indexMask;
        auto tail = tailIndex.load() & indexMask;

        if (tail >= head)
        {
            return tail - head;
        }

        return BufferSize - (head - tail);
    }

    bool Enqueue(const T& data)
    {
        const long tail = tailIndex.fetch_add(1);

        //while () {};

        if (buffer[tail & indexMask].isUsed.load())
        {
            return false;
        }

        buffer[tail & indexMask].data = data;
        if (buffer[tail & indexMask].isUsed.exchange(true))
        {
            __debugbreak();
        }
        return true;
    }

    bool Dequeue(T& data)
    {
        long head;
        for (;;)
        {
            head = headIndex.load();

            if (!buffer[head & indexMask].isUsed.load())
            {
                return false;
            }

            if (headIndex.compare_exchange_weak(head, head + 1))
            {
                break;
            }
        }
        data = std::move(buffer[head & indexMask].data);
        
        if constexpr (std::is_class_v<T> && !std::is_move_assignable_v<T>)
        {
            buffer[head & indexMask].~T();
        }
        
        if (!buffer[head & indexMask].isUsed.exchange(false))
        {
            __debugbreak();
        }
        return true;
    }

#pragma warning (disable : 4324)
    alignas( 64 ) std::atomic<long> headIndex = 0;
    alignas( 64 ) std::atomic<long> tailIndex = 0;
#pragma warning (default : 4324)
    int indexMask;
    Vector<Node> buffer;

public:
};
