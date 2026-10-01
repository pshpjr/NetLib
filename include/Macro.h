#pragma once
#include <mutex>
#include <shared_mutex>

#include "Utility.h"

/*--------------------
		DEBUG
---------------------*/

#if defined(DEBUG) || defined(_DEBUG)
#define PSH_DEBUG
#endif

#ifdef PSH_DEBUG
#define CRASH(cause)								\
	{																			\
		psh::util::DebugBreak();												\
	}

#define ASSERT_CRASH(expr,cause)			\
	do{																	\
		if (!(expr))												\
		{																\
			CRASH(cause);		\
		}																\
	}while(false)
#else

#define ASSERT_CRASH(expr,cause) do {} while (false)


#endif
#define OUT

/*---------------
	  Crash
---------------*/


/*--------------------
		Util Function
---------------------*/

#define MEMBER_SIZE(type, member) sizeof(((type *)0)->member)


/*-----------------
	Lock
 -----------------*/

#define USE_MANY_LOCKS(count)	std::shared_mutex _locks[count];
#define USE_LOCK				USE_MANY_LOCKS(1)
#define	READ_LOCK_IDX(idx)		std::shared_lock readLockGuard_##idx{_locks[idx]}
#define READ_LOCK				READ_LOCK_IDX(0)
#define	WRITE_LOCK_IDX(idx)		std::unique_lock writeLockGuard_##idx{_locks[idx]}
#define WRITE_LOCK				WRITE_LOCK_IDX(0)

/*-----------------
	Class
 -----------------*/

//매크로 설명
#define IS_INTERFACE(class_name)										\
public:																\
class_name() = default;											\
virtual ~class_name() = default;								\
// class_name(const class_name& other) = delete;					\
// class_name(class_name&& other) noexcept = delete;				\
// class_name& operator=(const class_name& other) = delete;		\
// class_name& operator=(class_name&& other) noexcept = delete;



#define DECLARE_PTR_IMPL(class_name, suffix) class class_name; using class_name##suffix = std::shared_ptr<class_name>;

//classRef 형태로 shared_ptr 정의.
#define DECLARE_PTR(class_name) DECLARE_PTR_IMPL(class_name,Ref)
