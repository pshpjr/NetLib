#pragma once
#include <cstddef>
#include <new.h>
class Node;

namespace psh
{
    /**
     * \brief 
     * \tparam Data 
     * \tparam DataId : short이내의 int 값이어야 함.  
     * \tparam UsePlacement  : 매번 생성자 호출할건지 여부. 기본값은 false
     */
    template <typename Data, int DataId, bool UsePlacement = false>
    class CMemoryPool
    {
        struct Node
        {
            Node* head = nullptr;
            Data data;
            Node* tail = nullptr;
        };

    public:
        explicit CMemoryPool(int iBlockNum);

        virtual ~CMemoryPool();


        Data* Alloc(void);


        bool Free(Data* pdata);

        int GetAllocCount(void) const
        {
            return _allocCount;
        }

        int GetObjectCount(void) const
        {
            return _objectCount;
        }

    private:
        Node* _pFreeNode = nullptr;
        int _objectCount = 0;
        int _allocCount = 0;
        static constexpr int IDENTIFIER = DataId << 16;
        static int _instanceCount;
    };

    template <typename data, int dataId, bool usePlacement>
    int CMemoryPool<data, dataId, usePlacement>::_instanceCount = 0;

    //placement가 false일 때 생성 속도가 좀 느려도(루프 두 번) 코드 깔끔한 게 더 좋을 것 같음. 
    template <typename data, int dataId, bool usePlacement>
    CMemoryPool<data, dataId, usePlacement>::CMemoryPool(const int iBlockNum)
    {
        for (int i = 0; i < iBlockNum; ++i)
        {
            Node* newNode = static_cast<Node*>(malloc(sizeof(Node)));

            newNode->tail = _pFreeNode;
            _pFreeNode = newNode;
        }

        if constexpr (usePlacement == false)
        {
            Node* pNode = _pFreeNode;
            while (pNode != nullptr)
            {
                new(pNode) Node();

                pNode = pNode->tail;
            }
        }

        _objectCount = iBlockNum;
        _allocCount = iBlockNum;
    }

    template <typename data, int dataId, bool usePlacement>
    CMemoryPool<data, dataId, usePlacement>::~CMemoryPool()
    {
        Node* node = _pFreeNode;
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
    data* CMemoryPool<data, dataId, usePlacement>::Alloc()
    {
        Node* retNode = _pFreeNode;
        if (retNode != nullptr)
        {
            _pFreeNode = _pFreeNode->tail;
            _objectCount--;

            if constexpr (usePlacement)
            {
                retNode = static_cast<Node*>(new(&retNode->data)data());
            }
        }
        else
        {
            _allocCount++;
            retNode = new Node;
        }
#ifdef MYDEBUG
		retNode->tail = (Node*)0x3412;
		retNode->_head = (Node*)0x3412;
#endif

        return static_cast<data*>(&(retNode->data));
    }

    template <typename data, int dataId, bool usePlacement>
    bool CMemoryPool<data, dataId, usePlacement>::Free(data* pdata)
    {
        Node* dataNode = static_cast<Node*>(static_cast<char*>(pdata) - offsetof(Node, data));

        //정상인지 테스트
#ifdef MYDEBUG
		if (dataNode->_head == (Node*)0x3412)
		{
			printf("correct\n");
		}
		if (dataNode->tail == (Node*)0x3412)
		{
			printf("correct\n");
		}
#endif

        if constexpr (usePlacement)
        {
            pdata->~data();
        }

        dataNode->tail = _pFreeNode;
        _pFreeNode = dataNode;

        _objectCount++;
        return true;
    }
}
