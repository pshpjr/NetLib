#pragma once
#include <mutex>

template <typename T>
class LockStack
{
    struct Node
    {
        Node* next = nullptr;
        T data = 0;
        Node() = default;

        Node(const T& data)
            : next(nullptr)
            , data(data)
        {
        }
    };

public:
    void Push(const T& data)
    {
        Node* node = new Node();

        node->data = data;

        std::lock_guard lock(_cs);
        node->next = _top;
        _top = node;
    }

    T Pop()
    {
        Node* t;
        {
            std::lock_guard lock(_cs);
            t = _top;
            _top = t->next;
        }

        T ret = t->data;
        delete t;
        return ret;
    }

private:
    Node* _top = nullptr;
    int _pooledNodeCount = 0;
    std::mutex _cs;
};
