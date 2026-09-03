#pragma once
#include <cstddef>
#include <new.h>
#include "Macro.h"
class Node;

template <typename Data, int DataId, bool UsePlacement>
class TlsPool;


/**
 * \brief
 * \tparam T
 * \tparam typeId : short이내의 int 값이어야 함.
 * \tparam usePlacement  : 매번 생성자 호출할건지 여부. 기본값은 false
 */
template <typename T, int typeId, bool usePlacement = false>
class SingleThreadObjectPool
{
    friend class TlsPool<T, typeId, usePlacement>;

public:
    struct Node
    {
        T data;
        Node* tail = nullptr;
    };


    explicit SingleThreadObjectPool(int iBlockNum);

    virtual ~SingleThreadObjectPool();

    template <typename... Args>
    T* Alloc(Args&&... args)
    {
        Node* top = _top;
        Node* retNode = top;
        if (top != nullptr)
        {
            _top = top->tail;
            --_objectCount;
            if constexpr (usePlacement)
            {
                retNode = static_cast<Node*>(new(&retNode->data)T(forward<Args>(args)...));
            }

            return &retNode->data;
        }

        _allocCount++;
        return &(new Node(T{forward<Args>(args)...}))->data;

#ifdef MYDEBUG
		retNode->_tail = ( Node* ) 0x3412;
		retNode->_head = ( Node* ) 0x3412;
#endif
    }


    bool Free(T* pdata);

    int GetAllocCount(void) const
    {
        return _allocCount;
    }

    int GetObjectCount(void) const
    {
        return _objectCount;
    }

    void SizeCheck() const
    {
        int size = 0;
        Node* cur = _top;
        while (cur != nullptr)
        {
            cur = cur->tail;
            size++;
        }
    }

private:
    alignas( 64 ) Node* _top = nullptr;
    int _objectCount = 0;
    int _allocCount = 0;
    static constexpr int _identifier = typeId << 16;
    static int _instanceCount;
};


template <typename Data, int DataId, bool UsePlacement>
int SingleThreadObjectPool<Data, DataId, UsePlacement>::_instanceCount = 0;

//placement가 false일 때 생성 속도가 좀 느려도(루프 두 번) 코드 깔끔한 게 더 좋을 것 같음.
template <typename data, int dataId, bool usePlacement>
SingleThreadObjectPool<data, dataId, usePlacement>::SingleThreadObjectPool(const int iBlockNum)
{
    for (int i = 0; i < iBlockNum; ++i)
    {
        Node* newNode = static_cast<Node*>(malloc(sizeof(Node)));

        if constexpr (usePlacement == false)
        {
            new(newNode) Node();
        }


        newNode->tail = _top;
        _top = newNode;
    }


    _objectCount = iBlockNum;
    _allocCount = iBlockNum;
}

template <typename data, int dataId, bool usePlacement>
SingleThreadObjectPool<data, dataId, usePlacement>::~SingleThreadObjectPool()
{
    Node* node = _top;
    while (node != nullptr)
    {
        Node* tar = node;
        node = node->tail;

        if constexpr (usePlacement)
        {
            free(tar);
        }
        else
        {
            delete tar;
        }
    }
}

template <typename data, int dataId, bool usePlacement>
bool SingleThreadObjectPool<data, dataId, usePlacement>::Free(data* pdata)
{
    Node* dataNode = reinterpret_cast<Node*>(reinterpret_cast<char*>(pdata) - offsetof(Node, data));

    //정상인지 테스트
#ifdef MYDEBUG
	if (dataNode->_head == (Node*)0x3412)
	{
		printf("correct\n");
	}
	if (dataNode->_tail == (Node*)0x3412)
	{
		printf("correct\n");
	}
#endif

    if constexpr (usePlacement)
    {
        pdata->~data();
    }

    dataNode->tail = _top;
    _top = dataNode;

    _objectCount++;

    //SizeCheck();


    return true;
}
