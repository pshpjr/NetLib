#pragma once
#include <cstddef>

class MemoryUsage
{
public:
    void Update();

    std::size_t GetPageFaultCount() const;
    std::size_t GetPeakPMemSize() const;
    std::size_t GetPMemSize() const;
    std::size_t GetPeakVirtualMemSize() const;
    std::size_t GetVirtualMemSize() const;

private:
    std::size_t _pageFaultCount = 0;
    std::size_t _peakPMemSize = 0;
    std::size_t _pMemSize = 0;
    std::size_t _peakVmemSize = 0;
    std::size_t _vMemsize = 0;
};