#pragma once
#include "SpinLock.h"

#include <memory>

template <typename T, bool usePlacement = false>
class SpinLockPool
{
public:

	struct Node
	{
	public:
		Node() :next(nullptr) {}
		Node(const T& data) :next(nullptr), _data(data) {}
		T _data;
		Node* next;
	};

	SpinLockPool(int iBlockNum)
	{

		for (int i = 0; i < iBlockNum; ++i)
		{
			Node* newNode = (Node*)malloc(sizeof(Node));

			newNode->next = _pFreeNode;
			_pFreeNode = newNode;
		}

		if constexpr (usePlacement == false)
		{
			Node* pNode = _pFreeNode;
			while (pNode != nullptr)
			{
				new(pNode) Node();

				pNode = pNode->next;
			}
		}

		_objectCount = iBlockNum;
		_allocCount = iBlockNum;
	}

	T* Alloc()
	{
		_lock.Lock();
		Node* retNode = _pFreeNode;
		if (retNode != nullptr)
		{
			_pFreeNode = _pFreeNode->next;
			_objectCount--;

			if constexpr (usePlacement)
			{
				retNode = (Node*)new(&retNode->_data)T();
			}
		}
		else
		{
			_allocCount++;
			_lock.UnLock();

			retNode = createNode();
			return (T*)(&(retNode->_data));
		}
#ifdef MYDEBUG
		retNode->next = (Node*)0x3412;
		retNode->_head = (Node*)0x3412;
#endif
		_lock.UnLock();
		return (T*)(&(retNode->_data));
	}

	bool Free(T* pdata)
	{
		Node* dataNode = (Node*)((char*)pdata - offsetof(Node, _data));

		//정상인지 테스트
#ifdef MYDEBUG
		if (dataNode->_head == (Node*)0x3412)
		{
			printf("correct\n");
		}
		if (dataNode->next == (Node*)0x3412)
		{
			printf("correct\n");
		}
#endif

		if constexpr (usePlacement)
		{
			pdata->~_data();
		}
		_lock.Lock();
		dataNode->next = _pFreeNode;
		_pFreeNode = dataNode;

		_objectCount++;
		_lock.UnLock();
		return true;
	}

private:
	Node* createNode()
	{
		Node* node = (Node*)malloc(sizeof(Node));
		node->next = nullptr;
		if constexpr (!usePlacement)
		{
			new(&node->_data) T();
		}
		return node;
	}

	Node* _pFreeNode = nullptr;
	int _objectCount = 0;
	int _allocCount = 0;

	SpinLock _lock;
};
