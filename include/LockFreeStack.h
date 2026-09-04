#pragma once
#include <atomic>
#include "MultiThreadObjectPool.h"
#include "LockFreeData.h"

template <typename T>
class LockFreeStack
{
    struct Node
    {
        T data{};
        Node* next = nullptr;
        Node() = default;

        Node(const T& data) : data(data)
                            , next(nullptr)
        {
        }
    };

public:
    LockFreeStack(): _pool(100)
    {
    }

    long size() const
    {
        return objectsInPool.load();
    }

    void Push(const T& data)
    {
        Node* node = _pool.Alloc();

        node->data = data;

        for (;;)
        {
            Node* top = _top.load();
            node->next = reinterpret_cast<Node*>((long long)top & lock_free_data::pointerMask);

            Node* newTop = reinterpret_cast<Node*>((unsigned long long)(node) | (
                                                  (long long)top + lock_free_data::indexInc &
                                                  lock_free_data::indexMask));

            if (_top.compare_exchange_strong(top, newTop))
            {
                ++objectsInPool;

                break;
            }
        }
    }

    bool Pop(T& data)
    {
        for (;;)
        {
            Node* top = _top.load();

            Node* topNode = reinterpret_cast<Node*>((unsigned long long)top & lock_free_data::pointerMask);
            if (topNode == nullptr)
            {
                return false;
            }


            data = topNode->data;
            Node* newTop = reinterpret_cast<Node*>(reinterpret_cast<long long>(topNode->next) |
                reinterpret_cast<long long>(top) & lock_free_data::indexMask);
            if (_top.compare_exchange_strong(top, newTop))
            {
                --objectsInPool;


                _pool.Free(topNode);


                break;
            }
        }
        return true;
    }

private:
    std::atomic<long> objectsInPool = 0;

    std::atomic<Node*> _top = nullptr;

    MultiThreadObjectPool<Node, false> _pool;
};
