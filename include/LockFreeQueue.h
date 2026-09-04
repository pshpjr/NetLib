#pragma once
#include <atomic>
#include "LockFreeData.h"
#include "MultiThreadObjectPool.h"


template <typename T>
class LockFreeQueue
{
    union Pointer
    {
        struct
        {
            short tmp1;
            short tmp2;
            short tmp3;
            short index;
        };

        long pointer;
    };


    struct Node
    {
        std::atomic<Node*> next = nullptr;
        T data;
        Node() = default;

        explicit Node(const T& data)
            : next(nullptr)
            , data(data)
        {
        }
    };

public:
    explicit LockFreeQueue(int basePoolSize)
        : _pool(basePoolSize)
    {
        Node* dummy = _pool.Alloc();

        dummy->next = nullptr;
        _head = dummy;
        _tail = dummy;
    }

    long Size() const
    {
        return _size.load();
    }

    int Enqueue(const T& data)
    {
        Node* newNode = _pool.Alloc();
        newNode->data = data;

        newNode->next = nullptr;
        const auto nextTailCount = ++tailCount;
        Node* newTail = static_cast<Node*>(static_cast<unsigned long long>(newNode) | static_cast<unsigned long long>(
            nextTailCount) << 47);


        int loop = 0;
        while (true)
        {
            Node* tail = _tail.load();
            Node* tailNode = static_cast<Node*>(static_cast<unsigned long long>(tail) & lock_free_data::_tailCount);

            if (tailNode->next.load() == nullptr)
            {
                Node* expected = nullptr;
                if (tailNode->next.compare_exchange_strong(expected, newTail))
                {
                    //auto debugCount = ++debugIndex;
                    //auto index = debugCount % debugSize;

                    //debug[index].threadID = std::this_thread::get_id();
                    //debug[index].type = IoTypes::Enqueue;
                    //debug[index].oldHead = (unsigned long long)tailNode;
                    //debug[index].newHead = (unsigned long long)((Node*)((unsigned long long)newTail &lock_free_data::_tailCount));

                    _tail.compare_exchange_strong(tail, newTail);

                    break;
                }
            }
            else
            {
                Node* next = tailNode->next.load();
                _tail.compare_exchange_strong(tail, next);
            }
            loop++;
        }
        ++_size;
        return loop;
    }

    int Dequeue(T& data)
    {
        if (_size == 0)
        {
            return -1;
        }

        int loopCount = 0;
        while (true)
        {
            Node* head = _head.load();
            Node* headNode = static_cast<Node*>(static_cast<unsigned long long>(head) & lock_free_data::_tailCount);

            Node* next = headNode->next.load();
            Node* nextNode = static_cast<Node*>(static_cast<unsigned long long>(next) & lock_free_data::_tailCount);

            if (next == nullptr)
            {
                return false;
            }

            data = nextNode->data;

            if (_head.compare_exchange_strong(head, next))
            {
                Node* tail = _tail.load();
                Node* tailNode = static_cast<Node*>(static_cast<unsigned long long>(tail) & lock_free_data::_tailCount);
                if (tail == head)
                {
                    //내가 뺀 애가 tail이면 풀에 넣기 전에 수정해줘야 함. 아니면 꼬임.
                    Node* nextTail = tailNode->next.load();
                    _tail.compare_exchange_strong(tail, nextTail);
                }

                _pool.Free(headNode);
                break;
            }
            loopCount++;
        }
        --_size;

        return loopCount;
    }

private:
    //이거 붙어있을 때 떨어져 있을 때 성능 차이 비교
    std::atomic<Node*> _head = nullptr;
    std::atomic<Node*> _tail = nullptr;
    //얘도 계속 수정됨. 따로 있을 때 비교.
    std::atomic<short> tailCount = 0;
    static long ID;
    std::atomic<long> _size = 0;
    MultiThreadObjectPool<Node> _pool;


    //DEBUG
    static constexpr int debugSize = 3000;
    long long debugIndex = 0;
    //debugData<T> debug[debugSize];
};

template <typename T>
long LockFreeQueue<T>::ID = 0;
