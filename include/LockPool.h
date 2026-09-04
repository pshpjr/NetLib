#pragma once
#include <memory>
#include <mutex>
template <typename T, bool usePlacement = false>
class LockPool
{
public:
	struct Node
	{
		T _data;
		Node* next;
		Node() :next(nullptr) {}
		Node(const T& data) :next(nullptr), _data(data) {}
	};

	LockPool(int iBlockNum)
	{
		for (int i = 0; i < iBlockNum; ++i)
		{
			Node* newNode = createNode();

			newNode->next = _pFreeNode;
			_pFreeNode = newNode;
		}

		_objectCount = iBlockNum;
		_allocCount = iBlockNum;
	}

	T* Alloc()
	{
		std::unique_lock lock(_lock);
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
			lock.unlock();

			retNode = createNode();
			return (T*)(&(retNode->_data));
		}
#ifdef MYDEBUG
		retNode->next = (Node*)0x3412;
		retNode->_head = (Node*)0x3412;
#endif
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
		std::lock_guard lock(_lock);
		dataNode->next = _pFreeNode;
		_pFreeNode = dataNode;

		_objectCount++;
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

	std::mutex _lock;
};
