#pragma once
#include <list>
#include "SingleThreadObjectPool.h"
#include "LockFreeStack.h"
#include "Utility.h"

constexpr int TLS_POOL_INITIAL_SIZE = 5'000;
constexpr int GLOBAL_POOL_MULTIPLIER = 10;

//#define TLS_LEAK_DEBUG
//#define TLS_DEBUG


template <typename T, int dataId, bool usePlacement = false>
class TlsPool
{
    using poolType = SingleThreadObjectPool<T, dataId, usePlacement>;
    using Node = typename poolType::Node;

public:
    int AllocatedCount() const
    {
        return (_acquireCount - _releaseCount) * _localPoolSize;
    }

    std::list<Node*> allocated;


    struct controlData
    {
        Node* front;
        Node* end;
    };

    TlsPool(const int localPoolSize, const int globalPoolMultiplier):
                                                                    _poolID(InterlockedIncrement(&gPoolID))
                                                                    , _localPoolSize(localPoolSize)
                                                                    , _globalPoolMultiplier(globalPoolMultiplier)
    {
#ifdef TLS_LEAK_DEBUG
		InitializeCriticalSection(&_debugLock);
#endif

        InitializeCriticalSection(&_cs);
        psh::util::TlsAlloc(_localPoolTlsIndex);

        _pooledNodeSize = _globalPoolMultiplier;

        for (int j = 0; j < _globalPoolMultiplier; j++)
        {
            _nextBucket.Push(MakeControl());
        }
    }

    int GetGPoolSize() const
    {
        return _pooledNodeSize;
    }

    int GetGPoolEmptyCount() const
    {
        return _poolEmptyCount;
    }

    int GetAcquireCount() const
    {
        return _acquireCount;
    }

    int GetReleaseCount() const
    {
        return _releaseCount;
    }

    Node* createNode()
    {
        Node* newNode = static_cast<Node*>(malloc(sizeof(Node)));
        __analysis_assume(newNode != nullptr);
        newNode->tail = nullptr;

#ifdef TLS_LEAK_DEBUG
		EnterCriticalSection(&_debugLock);
		allocated.push_back(newNode);
		LeaveCriticalSection(&_debugLock);
#endif

        if constexpr (!usePlacement)
        {
            new(&newNode->data) T();
        }
        return newNode;
    }

    /**
     * \brief 노드 쓰는 각종 작업에서 리턴하는 건 뭉치의 가장 처음 노드. 
     * \return 
     */
    controlData AcquireNode(const int size)
    {
        InterlockedIncrement(&_acquireCount);


        controlData data;

        if (!_nextBucket.Pop(data))
        {
            auto cb1 = MakeControl();
            auto retCb = MakeControl();

            _nextBucket.Push(cb1);

            InterlockedIncrement(&_poolEmptyCount);
            InterlockedAdd(&_pooledNodeSize, size);
            return retCb;
        }
        InterlockedAdd(&_pooledNodeSize, -size);

        return data;
    }


    void ReleaseNode(Node* head, Node* tail, const int size)
    {
        controlData cb{};
        cb.front = head;
        cb.end = tail;

        _nextBucket.Push(cb);

        InterlockedIncrement(&_releaseCount);
        InterlockedAdd(&_pooledNodeSize, size);
    }


    T* Alloc()
    {
        poolType* pool = static_cast<poolType*>(psh::util::TlsGet(_localPoolTlsIndex));

        if (pool == nullptr)
        {
            pool = new poolType(0);
            psh::util::TlsSet(_localPoolTlsIndex, pool);
        }

        if (pool->GetObjectCount() == 0)
        {
            auto cb = AcquireNode(1);

            pool->_top = cb.front;
            pool->_objectCount = _localPoolSize;
        }
        return pool->Alloc();
    }

    void Free(T* data)
    {
        //PRO_BEGIN("FREE")
        poolType* pool = static_cast<poolType*>(psh::util::TlsGet(_localPoolTlsIndex));

        if (pool == nullptr)
        {
            pool = new poolType(0);
            psh::util::TlsSet(_localPoolTlsIndex, pool);
        }

        pool->Free(data);

        if (pool->GetObjectCount() > _localPoolSize * 2)
        {
            //PRO_BEGIN("FREE_RELEASE")
            Node* releaseHead = pool->_top;

            Node* releaseTail = pool->_top;

            for (int i = 0; i < _localPoolSize - 1; ++i)
            {
                releaseTail = releaseTail->tail;
            }

            Node* newHead = releaseTail->tail;
            releaseTail->tail = nullptr;


            pool->_top = newHead;

            ReleaseNode(releaseHead, releaseTail, 1);

            pool->_objectCount -= _localPoolSize;
        }
    }

    controlData MakeControl()
    {
        Node* newTop = static_cast<Node*>(malloc(sizeof(Node) * _localPoolSize));
        controlData data = {};
        data.front = newTop;

        Node* newNode = newTop;
        for (int i = 0; i < _localPoolSize - 1; ++i)
        {
            new(newNode)Node();
            newNode->tail = newNode + 1;
            newNode += 1;
        }
        new(newNode)Node();
        newNode->tail = nullptr;
        data.end = newNode;

        return data;
    }

private:
    CRITICAL_SECTION _cs;

    LockFreeStack<controlData> _nextBucket;

    inline static psh::util::TlsKey _localPoolTlsIndex = 0;
    Node* _top = nullptr;
    long _pooledNodeSize = 0;
    inline static long gPoolID;
    long _poolID = 0;
    int _localPoolSize;
    int _globalPoolMultiplier;

    long _acquireCount = 0;
    long _releaseCount = 0;

    unsigned int _poolEmptyCount = 0;
#ifdef TLS_DEBUG
	struct Debug
	{
		int threadID;
		int type;
		int globalPoolSize;
		long long globalPoolAddr;
		int localPoolSize;
		long long localPoolAddr;
	};
	long debugIndex = 0;
	Debug debug[1000];

#endif

#ifdef TLS_LEAK_DEBUG

	CRITICAL_SECTION _debugLock;

#endif
};

#ifndef USE_TLS_POOL

#define USE_TLS_POOL(CLASS) 	friend TlsPool<CLASS, 0, false>; friend SingleThreadObjectPool<CLASS, 0, false>;

#endif
