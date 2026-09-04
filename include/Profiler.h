#pragma once

#include <mutex>
#include <shared_mutex>

#ifdef NETLIB_ENABLE_TRACY
#include <tracy/Tracy.hpp>

#define PRO_BEGIN(name) ZoneScopedN(name)
#define PRO_FUNCTION() ZoneScoped
#define PRO_THREAD(name) tracy::SetThreadName(name)
#define PRO_MESSAGE(message) TracyMessageL(message)
#define PRO_PLOT(name, value) TracyPlot(name, value)
#define PRO_LOCK(type, name) TracyLockable(type, name)
#define PRO_SHARED_LOCK(type, name) TracySharedLockable(type, name)
#else
#define PRO_BEGIN(name)
#define PRO_FUNCTION()
#define PRO_THREAD(name)
#define PRO_MESSAGE(message)
#define PRO_PLOT(name, value)
#define PRO_LOCK(type, name) type name
#define PRO_SHARED_LOCK(type, name) type name
#endif
