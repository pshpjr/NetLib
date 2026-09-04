//
// Created by pshpjr on 26. 9. 4..
//

#include "ThreadMonitor.h"

#ifdef _WIN32
#include "MyWindows.h"
#else
#include <sys/resource.h>
#endif

namespace
{
#ifdef _WIN32
psh::uint64 ToMilliseconds(const FILETIME& time)
{
    return ((static_cast<psh::uint64>(time.dwHighDateTime) << 32) | time.dwLowDateTime) / 10'000;
}
#else
psh::uint64 ToMilliseconds(const timeval& time)
{
    return static_cast<psh::uint64>(time.tv_sec) * 1'000
        + static_cast<psh::uint64>(time.tv_usec) / 1'000;
}
#endif
}

void ThreadMonitor::Update()
{
    psh::uint64 kernelTimeMs;
    psh::uint64 userTimeMs;

#ifdef _WIN32
    FILETIME creationTime, exitTime, kernelTime, userTime;
    if (!GetThreadTimes(_thread.native_handle(), &creationTime, &exitTime, &kernelTime, &userTime))
        return;

    kernelTimeMs = ToMilliseconds(kernelTime);
    userTimeMs = ToMilliseconds(userTime);
#else
    if (_thread.get_id() != std::this_thread::get_id())
        return;

    rusage usage{};
    if (getrusage(RUSAGE_THREAD, &usage) != 0)
        return;

    kernelTimeMs = ToMilliseconds(usage.ru_stime);
    userTimeMs = ToMilliseconds(usage.ru_utime);
#endif

    if (_hasPreviousSample)
    {
        _kernelTimeMs = kernelTimeMs - _lastKernelTimeMs;
        _userTimeMs = userTimeMs - _lastUserTimeMs;
    }

    _lastKernelTimeMs = kernelTimeMs;
    _lastUserTimeMs = userTimeMs;
    _hasPreviousSample = true;
}
