#pragma once

#include <cstddef>

class MemoryUsage
{
public:
    void Update();

    size_t GetPageFaultCount() const;
    size_t GetPeakPMemSize() const;
    size_t GetPMemSize() const;
    size_t GetPeakVirtualMemSize() const;
    size_t GetVirtualMemSize() const;
    //  Paged/NonPaged pool은 Windows에만 있다. 다른 OS에서는 0
    size_t GetPeakPagedPoolUsage() const;
    size_t GetPeakNonPagedPoolUsage() const;
    size_t GetPagedPoolUsage() const;
    size_t GetNonPagedPoolUsage() const;

private:
    size_t _pageFaultCount = 0;
    size_t _peakPMemSize = 0;
    size_t _pMemSize = 0;
    size_t _peakVmemSize = 0;
    size_t _vMemsize = 0;

    size_t _peakPagedPoolUsage = 0;
    size_t _peakNonPagedPoolUsage = 0;
    size_t _pagedPoolUsage = 0;
    size_t _nonPagedPoolUsage = 0;
};
