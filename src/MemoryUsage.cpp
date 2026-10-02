#include "MemoryUsage.h"
#include "stdafx.h"

#ifdef _WIN32
#include "MyWindows.h"
#include <Psapi.h>
#else
#include <sys/resource.h>

#include <charconv>
#include <fstream>
#include <string>
#include <string_view>
#endif


#ifdef _WIN32
void MemoryUsage::Update()
{
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
    {
        _pageFaultCount = pmc.PageFaultCount;

        _peakPagedPoolUsage = pmc.QuotaPeakPagedPoolUsage;
        _peakNonPagedPoolUsage = pmc.QuotaPeakNonPagedPoolUsage;
        _pagedPoolUsage = pmc.QuotaPagedPoolUsage;
        _nonPagedPoolUsage = pmc.QuotaNonPagedPoolUsage;

        _peakPMemSize = pmc.PeakWorkingSetSize;
        _peakVmemSize = pmc.PeakPagefileUsage;

        _pMemSize = pmc.WorkingSetSize;
        _vMemsize = pmc.PagefileUsage;
    }
    else
    {
    }
}
#else
void MemoryUsage::Update()
{
    //  Windows의 PageFaultCount는 soft fault를 포함한다.
    rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) == 0)
    {
        _pageFaultCount = static_cast<size_t>(usage.ru_minflt + usage.ru_majflt);
    }

    //  /proc/self/status의 값은 kB 단위다.
    std::ifstream status("/proc/self/status");
    std::string line;
    while (std::getline(status, line))
    {
        const auto colon = line.find(':');
        if (colon == std::string::npos)
            continue;

        size_t* target = nullptr;
        const std::string_view key(line.data(), colon);
        if (key == "VmPeak") target = &_peakVmemSize;
        else if (key == "VmSize") target = &_vMemsize;
        else if (key == "VmHWM") target = &_peakPMemSize;
        else if (key == "VmRSS") target = &_pMemSize;
        else continue;

        const auto begin = line.find_first_not_of(" \t", colon + 1);
        if (begin == std::string::npos)
            continue;

        size_t valueKb = 0;
        const auto [ptr, error] = std::from_chars(line.data() + begin, line.data() + line.size(), valueKb);
        if (error == std::errc{})
            *target = valueKb * 1024;
    }
}
#endif

size_t MemoryUsage::GetPageFaultCount() const
{
    return _pageFaultCount;
}

size_t MemoryUsage::GetPeakPMemSize() const
{
    return _peakPMemSize;
}

size_t MemoryUsage::GetPMemSize() const
{
    return _pMemSize;
}

size_t MemoryUsage::GetPeakVirtualMemSize() const
{
    return _peakVmemSize;
}

size_t MemoryUsage::GetVirtualMemSize() const
{
    return _vMemsize;
}

size_t MemoryUsage::GetPeakPagedPoolUsage() const
{
    return _peakPagedPoolUsage;
}

size_t MemoryUsage::GetPeakNonPagedPoolUsage() const
{
    return _peakNonPagedPoolUsage;
}

size_t MemoryUsage::GetPagedPoolUsage() const
{
    return _pagedPoolUsage;
}

size_t MemoryUsage::GetNonPagedPoolUsage() const
{
    return _nonPagedPoolUsage;
}
