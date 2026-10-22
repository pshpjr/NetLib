#pragma once

#include <mutex>
#include <shared_mutex>

#ifdef NETLIB_ENABLE_TRACY
//	liburing이 끌어오는 <linux/fs.h>의 BLOCK_SIZE 매크로가 Tracy 큐의 같은 이름 멤버를 깨뜨린다.
#pragma push_macro("BLOCK_SIZE")
#undef BLOCK_SIZE
#include <tracy/Tracy.hpp>
#pragma pop_macro("BLOCK_SIZE")

#define PRO_BEGIN(name) ZoneScopedN(name)
#define PRO_FUNCTION() ZoneScoped
#define PRO_FRAME(name) FrameMarkNamed(name)
#define PRO_THREAD(name) tracy::SetThreadName(name)
#define PRO_VALUE(value) ZoneValue(value)
#define PRO_MESSAGE(message) TracyMessageL(message)
#define PRO_PLOT(name, value) TracyPlot(name, value)
#define PRO_LOCK(type, name) TracyLockable(type, name)
#define PRO_SHARED_LOCK(type, name) TracySharedLockable(type, name)
#else
#define PRO_BEGIN(name)
#define PRO_FUNCTION()
#define PRO_FRAME(name)
#define PRO_THREAD(name)
#define PRO_VALUE(value)
#define PRO_MESSAGE(message)
#define PRO_PLOT(name, value)
#define PRO_LOCK(type, name) type name
#define PRO_SHARED_LOCK(type, name) type name
#endif
