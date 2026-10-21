#pragma once
#include "Container.h"
#include "Macro.h"
#include <cstdint>
#include <atomic>
#include <utility>
//  [주의]: 초당 100만개 처리 기준 한 번에 58만년 이상 사용하지 마시오
//  큐 생성 시 T를 생성하며, dequeue 시 기존 데이터의 소멸자를 호출하지 않음.
template <typename T, int BufferSize>
class LockFreeFixedQueue
{
    //  2^n 형태여야 한다.
    static_assert((BufferSize & (BufferSize - 1)) == 0);
    struct Node
    {
        static constexpr std::uint64_t IndexMask = BufferSize - 1;
        std::atomic<std::uint64_t> sequence{};
        T data = T();
    };
public:
    constexpr LockFreeFixedQueue(): buffer(BufferSize)
    {
        for (std::size_t i = 0; i < BufferSize; ++i)
        {
            buffer[i].sequence.store(i, std::memory_order::relaxed);
        }
    }
    ~LockFreeFixedQueue() = default;
    LockFreeFixedQueue(const LockFreeFixedQueue& other) = delete;
    LockFreeFixedQueue(LockFreeFixedQueue&& other) = delete;
    LockFreeFixedQueue& operator =(const LockFreeFixedQueue& other) = delete;
    LockFreeFixedQueue& operator =(LockFreeFixedQueue&& other) = delete;
    bool Enqueue(const T& data)
    {
        T value(data);
        return EnqueueImpl(std::move(value));
    }
    bool Enqueue(T&& data)
    {
        T value(std::move(data));
        return EnqueueImpl(std::move(value));
    }
    bool Dequeue(T& data)
    {
        for (;;)
        {
            auto head = headIndex.load(std::memory_order::relaxed);
            auto& node = buffer[head & Node::IndexMask];
            auto sequence = node.sequence.load(std::memory_order::acquire);
            // 아직 데이터가 안 들어옴
            if (sequence < head + 1)
                return false;
            // 다른 Consumer가 먼저 진행한 경우
            if (sequence != head + 1)
                continue;
            //  독점 권한을 얻음
            if (!headIndex.compare_exchange_weak(head, head + 1, std::memory_order::relaxed, std::memory_order::relaxed))
                continue;
            data = std::move(node.data);
            node.sequence.store(head + BufferSize, std::memory_order::relaxed);
            return true;
        }
    }
    int Size() const
    {
        auto head = headIndex.load(std::memory_order::relaxed);
        auto tail = tailIndex.load(std::memory_order::relaxed);
        return tail - head;
    }
    void Clear()
    {
        headIndex.store(0, std::memory_order::relaxed);
        tailIndex.store(0, std::memory_order::relaxed);
        for (std::size_t i = 0; i < BufferSize; ++i)
        {
            buffer[i].sequence.store(i, std::memory_order::relaxed);
        }
    }
private:
    bool EnqueueImpl(T&& data)
    {
        for (;;)
        {
            auto tail = tailIndex.load();
            auto& node = buffer[tail & Node::IndexMask];
            auto sequence = node.sequence.load(std::memory_order::acquire);
            //  1. 슬롯 소유자가 없다.
            if (sequence == tail)
            {
                if (!tailIndex.compare_exchange_weak(tail, tail + 1, std::memory_order::relaxed, std::memory_order::relaxed))
                    continue;
                //  이 시점에서 나만 접근 가능하다.
                node.data = std::move(data);
                //  +1 하면 값을 넣었다는 뜻이다.
                node.sequence.store(tail + 1, std::memory_order::release);
                return true;
            }
            //  2. 슬롯 소유자가 있는데 한 바퀴 돌아서 온 상황.
            //  큐가 전부 예약 혹은 값이 있음. 추가 불가
            if (sequence < tail)
                return false;
            //  3. 슬롯을 누가 소유한 경우 최신값으로 재시도
        }
    }
#pragma warning (disable : 4324)
    alignas(64) std::atomic<uint64_t> headIndex = 0;
    alignas(64) std::atomic<uint64_t> tailIndex = 0;
#pragma warning (default : 4324)
    Vector<Node> buffer;
};
