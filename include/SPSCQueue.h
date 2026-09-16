//
// Created by pshpj on 24. 10. 17.
//

#ifndef SPSCQUEUE_H
#define SPSCQUEUE_H

template <typename T, int BufferSize>
class SPSCQueue
{
    static_assert((BufferSize & (BufferSize - 1)) == 0, "BufferSize must be a power of 2");
public:
    constexpr SPSCQueue()
        : indexMask(BufferSize - 1)
        , buffer(BufferSize) {}

    ~SPSCQueue() = default;

    SPSCQueue(const SPSCQueue& other) = delete;
    SPSCQueue(SPSCQueue&& other) = delete;

    SPSCQueue& operator=(const SPSCQueue& other) = delete;
    SPSCQueue& operator=(SPSCQueue&& other) = delete;


    int Size() const
    {
        return static_cast<int>((tailIndex - headIndex) & indexMask);
    }


    bool IsEmpty() const
    {
        return (tailIndex == headIndex);
    }


    bool IsFull() const
    {
        return ((tailIndex - headIndex) & indexMask) == indexMask;
    }

    bool Enqueue(const T& data)
    {
        if (IsFull())
        {
            return false;
        }

        buffer[tailIndex & indexMask].data = data;

        tailIndex = tailIndex + 1;

        return true;
    }

    bool Dequeue(T& data)
    {
        if (IsEmpty())
        {
            return false;
        }

        data = std::move(buffer[headIndex & indexMask].data);

        headIndex = headIndex + 1;

        return true;
    }
private:

    struct Node
    {
        T data = T();
    };
    uint64_t headIndex = 0;
    uint64_t tailIndex = 0;
    const int indexMask;
    std::vector<Node> buffer;
};
#endif //SPSCQUEUE_H
