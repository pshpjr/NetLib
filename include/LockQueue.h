//
// Created by pshpj on 24. 10. 15.
//

#ifndef LOCKQUEUE_H
#define LOCKQUEUE_H
#pragma once
#include <array>
#include <mutex>
#include <type_traits>
#include <utility>

#include <cassert>

// LockBasedFixedQueue: 락을 사용하여 구현된 고정 크기 큐
template <typename T, int BufferSize>
class LockBasedFixedQueue
{
    static_assert((BufferSize & (BufferSize - 1)) == 0, "BufferSize must be a power of two");
public:
    LockBasedFixedQueue() = default;

    ~LockBasedFixedQueue() = default;

    // 복사 및 이동 생성자/할당 연산자 삭제
    LockBasedFixedQueue(const LockBasedFixedQueue& other) = delete;
    LockBasedFixedQueue(LockBasedFixedQueue&& other) = delete;

    LockBasedFixedQueue& operator =(const LockBasedFixedQueue& other) = delete;
    LockBasedFixedQueue& operator =(LockBasedFixedQueue&& other) = delete;

    // 현재 큐의 크기를 반환
    int Size()
    {
        std::lock_guard myLock(_mutex);
        if (tailIndex >= headIndex)
        {
            return tailIndex - headIndex;
        }
        return BufferSize - (headIndex - tailIndex);
    }

    // 큐에 데이터를 삽입
    bool Enqueue(const T& data)
    {


        std::lock_guard myLock(_mutex);
        if (size() >= BufferSize)
        {
            // 큐가 가득 참
            return false;
        }

        buffer[tailIndex & indexMask].data = data;
        tailIndex++;
        return true;
    }

    // 큐에서 데이터를 제거하고 반환
    bool Dequeue(T& data)
    {
        std::lock_guard myLock(_mutex);
        if (size() == 0)
        {
            // 큐가 비어 있음
            return false;
        }

        data = std::move(buffer[headIndex & indexMask]).data;

        headIndex++;
        return true;
    }
private:

    struct Node
    {
        T data;
    };

    int size() const
    {
        if (tailIndex >= headIndex)
        {
            return tailIndex - headIndex;
        }
        return BufferSize - (headIndex - tailIndex);
    }
    const int indexMask{BufferSize - 1};
    std::array<Node, BufferSize> buffer{};
    std::mutex _mutex; // 큐 접근을 보호하는 뮤텍스
    long headIndex{0};
    long tailIndex{0};

    // 동일한 인터페이스 유지를 위해 공개된 멤버 없음
};


#endif //LOCKQUEUE_H
