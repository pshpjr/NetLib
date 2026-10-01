#include "CpuUsage.h"

#include <chrono>
#include <cstdint>

#ifdef _WIN32
#include "MyWindows.h"
#else
#include <fstream>
#include <string>
#include <sys/resource.h>
#include <unistd.h>
#endif

namespace
{
    struct CpuTimes
    {
        std::uint64_t total = 0;
        std::uint64_t kernel = 0;
        std::uint64_t user = 0;
    };

#ifdef _WIN32
    std::uint64_t ToTicks(const FILETIME& time)
    {
        return (static_cast<std::uint64_t>(time.dwHighDateTime) << 32) | time.dwLowDateTime;
    }

    bool ReadSystemTimes(CpuTimes& times)
    {
        FILETIME idle, kernel, user;
        if (!GetSystemTimes(&idle, &kernel, &user))
        {
            return false;
        }
        const auto kernelTicks = ToTicks(kernel);
        const auto idleTicks = ToTicks(idle);
        if (kernelTicks < idleTicks)
        {
            return false;
        }
        times.user = ToTicks(user);
        times.kernel = kernelTicks - idleTicks;
        times.total = kernelTicks + times.user;
        return true;
    }

    bool ReadProcessTimes(CpuTimes& times)
    {
        FILETIME creation, exit, kernel, user;
        if (!GetProcessTimes(GetCurrentProcess(), &creation, &exit, &kernel, &user))
        {
            return false;
        }
        // Process CPU and the steady-clock interval are both measured in nanoseconds.
        times.kernel = ToTicks(kernel) * 100;
        times.user = ToTicks(user) * 100;
        return true;
    }
#else
    bool ReadSystemTimes(CpuTimes& times)
    {
        std::ifstream stat("/proc/stat");
        std::string label;
        std::uint64_t user, nice, system, idle, iowait, irq, softirq, steal;
        if (!(stat >> label >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal) || label != "cpu")
        {
            return false;
        }
        // Guest times are already included in user/nice. I/O wait is idle time; steal is only in the denominator.
        times.user = user + nice;
        times.kernel = system + irq + softirq;
        times.total = times.user + times.kernel + idle + iowait + steal;
        return true;
    }

    std::uint64_t ToNanoseconds(const timeval& time)
    {
        return static_cast<std::uint64_t>(time.tv_sec) * 1'000'000'000 +
               static_cast<std::uint64_t>(time.tv_usec) * 1'000;
    }

    bool ReadProcessTimes(CpuTimes& times)
    {
        rusage usage{};
        if (getrusage(RUSAGE_SELF, &usage) != 0)
        {
            return false;
        }
        times.kernel = ToNanoseconds(usage.ru_stime);
        times.user = ToNanoseconds(usage.ru_utime);
        return true;
    }
#endif
}

CpuUsage::CpuUsage()
{
#ifdef _WIN32
    SYSTEM_INFO systemInfo{};
    GetSystemInfo(&systemInfo);
    if (systemInfo.dwNumberOfProcessors > 0)
    {
        _numberOfProcessors = systemInfo.dwNumberOfProcessors;
    }
#else
    const auto processors = sysconf(_SC_NPROCESSORS_ONLN);
    if (processors > 0)
    {
        _numberOfProcessors = static_cast<unsigned int>(processors);
    }
#endif
    UpdateCpuTime();
}

void CpuUsage::UpdateCpuTime()
{
    CpuTimes processor;
    if (ReadSystemTimes(processor))
    {
        if (_hasProcessorSample && processor.total > _processorLastTotal &&
            processor.user >= _processorLastUser && processor.kernel >= _processorLastKernel)
        {
            const auto totalDiff = static_cast<double>(processor.total - _processorLastTotal);
            _fProcessorUser = static_cast<float>((processor.user - _processorLastUser) / totalDiff * 100.0);
            _fProcessorKernel = static_cast<float>((processor.kernel - _processorLastKernel) / totalDiff * 100.0);
            _fProcessorTotal = _fProcessorUser + _fProcessorKernel;
        }
        else
        {
            _fProcessorTotal = _fProcessorUser = _fProcessorKernel = 0;
        }
        _processorLastTotal = processor.total;
        _processorLastKernel = processor.kernel;
        _processorLastUser = processor.user;
        _hasProcessorSample = true;
    }

    CpuTimes process;
    if (ReadProcessTimes(process))
    {
        const auto now = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
        if (_hasProcessSample && now > _processLastTime && process.kernel >= _processLastKernel &&
            process.user >= _processLastUser)
        {
            const auto totalDiff = static_cast<double>(now - _processLastTime) * _numberOfProcessors;
            _fProcessKernel = static_cast<float>((process.kernel - _processLastKernel) / totalDiff * 100.0);
            _fProcessUser = static_cast<float>((process.user - _processLastUser) / totalDiff * 100.0);
            _fProcessTotal = _fProcessKernel + _fProcessUser;
        }
        else
        {
            _fProcessTotal = _fProcessUser = _fProcessKernel = 0;
        }
        _processLastTime = now;
        _processLastKernel = process.kernel;
        _processLastUser = process.user;
        _hasProcessSample = true;
    }
}
