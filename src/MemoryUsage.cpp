#include "MemoryUsage.h"

#ifdef _WIN32
#include "MyWindows.h"
#include <Psapi.h>
#else
#include <fstream>
#include <sstream>
#include <string>

namespace
{
struct ProcMemoryUsage
{
    std::size_t peakPhysical = 0;
    std::size_t physical = 0;
    std::size_t peakVirtual = 0;
    std::size_t virtualSize = 0;
};

bool ReadPageFaultCount(std::size_t& pageFaultCount)
{
    std::ifstream stat("/proc/self/stat");
    std::string line;
    if (!std::getline(stat, line))
    {
        return false;
    }

    const std::size_t commandEnd = line.rfind(')');
    if (commandEnd == std::string::npos)
    {
        return false;
    }

    std::istringstream fields{line.substr(commandEnd + 1)};
    char state;
    unsigned long long ignored;
    unsigned long long minorFaults;
    unsigned long long majorFaults;

    if (!(fields >> state))
    {
        return false;
    }

    for (int field = 4; field <= 9; ++field)
    {
        if (!(fields >> ignored))
        {
            return false;
        }
    }

    if (!(fields >> minorFaults >> ignored >> majorFaults))
    {
        return false;
    }

    pageFaultCount = static_cast<std::size_t>(minorFaults + majorFaults);
    return true;
}

bool ReadMemoryUsage(ProcMemoryUsage& usage)
{
    std::ifstream status("/proc/self/status");
    std::string line;
    bool foundPeakPhysical = false;
    bool foundPhysical = false;
    bool foundPeakVirtual = false;
    bool foundVirtual = false;

    while (std::getline(status, line))
    {
        std::istringstream fields{line};
        std::string key;
        fields >> key;

        std::size_t* destination = nullptr;
        bool* found = nullptr;
        if (key == "VmHWM:")
        {
            destination = &usage.peakPhysical;
            found = &foundPeakPhysical;
        }
        else if (key == "VmRSS:")
        {
            destination = &usage.physical;
            found = &foundPhysical;
        }
        else if (key == "VmPeak:")
        {
            destination = &usage.peakVirtual;
            found = &foundPeakVirtual;
        }
        else if (key == "VmSize:")
        {
            destination = &usage.virtualSize;
            found = &foundVirtual;
        }
        else
        {
            continue;
        }

        unsigned long long kibibytes;
        std::string unit;
        if (!(fields >> kibibytes >> unit) || unit != "kB")
        {
            return false;
        }

        *destination = static_cast<std::size_t>(kibibytes * 1024);
        *found = true;
    }

    return foundPeakPhysical && foundPhysical && foundPeakVirtual && foundVirtual;
}
}
#endif

void MemoryUsage::Update()
{
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc{};
    if (K32GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
    {
        _pageFaultCount = pmc.PageFaultCount;
        _peakPMemSize = pmc.PeakWorkingSetSize;
        _pMemSize = pmc.WorkingSetSize;
        _peakVmemSize = pmc.PeakPagefileUsage;
        _vMemsize = pmc.PagefileUsage;
    }
#else
    ReadPageFaultCount(_pageFaultCount);

    ProcMemoryUsage usage;
    if (ReadMemoryUsage(usage))
    {
        _peakPMemSize = usage.peakPhysical;
        _pMemSize = usage.physical;
        _peakVmemSize = usage.peakVirtual;
        _vMemsize = usage.virtualSize;
    }
#endif
}

std::size_t MemoryUsage::GetPageFaultCount() const
{
    return _pageFaultCount;
}

std::size_t MemoryUsage::GetPeakPMemSize() const
{
    return _peakPMemSize;
}

std::size_t MemoryUsage::GetPMemSize() const
{
    return _pMemSize;
}

std::size_t MemoryUsage::GetPeakVirtualMemSize() const
{
    return _peakVmemSize;
}

std::size_t MemoryUsage::GetVirtualMemSize() const
{
    return _vMemsize;
}