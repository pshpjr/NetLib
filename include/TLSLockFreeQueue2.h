#pragma once
#include "Container.h"
#include "Utility.h"
#include <TLSLockFreeQueue.h>

template <typename T>
class TLSLockFreeQueue2
{
public:
	TLSLockFreeQueue2()
	{
		psh::util::TlsAlloc(_tlsIndex);
		InitializeSRWLock(&_queueListLock);
	}

	~TLSLockFreeQueue2() { psh::util::TlsFree(_tlsIndex); }

	void enqueue(T data)
	{
		TLSLockFreeQueue<T>* queue;
		{
			queue = ( TLSLockFreeQueue<T>* ) psh::util::TlsGet(_tlsIndex);
		}

		if ( queue == nullptr )
		{

			queue = new TLSLockFreeQueue<T>();
			psh::util::TlsSet(_tlsIndex,queue);

			AcquireSRWLockExclusive(&_queueListLock);

			queues.push_back(queue);

			ReleaseSRWLockExclusive(&_queueListLock);

			InterlockedIncrement(&_queueCount);
		}


		queue->Enqueue(data);
	}

	auto nextQueue(typename std::vector<TLSLockFreeQueue<T>*>::iterator i)
	{
		return std::next(i) != queues.end() ? std::next(i) : queues.begin();
	}

	bool dequeue(T& data)
	{
		int queueCount = _queueCount;

		auto i = queues.begin() + lastQueueIndex;
		bool find = false;
		AcquireSRWLockShared(&_queueListLock);
		for ( int n = 0; n < queueCount; ++n, i = nextQueue(i) )
		{
			if ( (*i)->Dequeue(data) )
			{
				find = true;
				lastQueueIndex = ( lastQueueIndex + n ) % queueCount;
				break;
			}
		}
		ReleaseSRWLockShared(&_queueListLock);
		return find;
	}


private:
	inline static thread_local int lastQueueIndex = 0;
	psh::util::TlsKey _tlsIndex;
	long _queueCount = 0;

	std::vector<TLSLockFreeQueue<T>*> queues;
	SRWLOCK _queueListLock;
};

