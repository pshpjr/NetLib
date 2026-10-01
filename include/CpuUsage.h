#pragma once

#include <cstdint>

// CPU percentages for the system and the current process, normalized to all logical processors.
// Construction establishes a baseline; call UpdateCpuTime() every 500-1000 ms for a useful sample.
class CpuUsage
{
public:
    CpuUsage();
    void UpdateCpuTime();

    float ProcessorTotal() const
    {
        return _fProcessorTotal;
    }

    float ProcessorUser() const
    {
        return _fProcessorUser;
    }

    float ProcessorKernel() const
    {
        return _fProcessorKernel;
    }

    float ProcessTotal() const
    {
        return _fProcessTotal;
    }

    float ProcessUser() const
    {
        return _fProcessUser;
    }

    float ProcessKernel() const
    {
        return _fProcessKernel;
    }

private:
    unsigned int _numberOfProcessors = 1;
    float _fProcessorTotal = 0;
    float _fProcessorUser = 0;
    float _fProcessorKernel = 0;
    float _fProcessTotal = 0;
    float _fProcessUser = 0;
    float _fProcessKernel = 0;
    bool _hasProcessorSample = false;
    bool _hasProcessSample = false;
    std::uint64_t _processorLastTotal = 0;
    std::uint64_t _processorLastKernel = 0;
    std::uint64_t _processorLastUser = 0;
    std::uint64_t _processLastKernel = 0;
    std::uint64_t _processLastUser = 0;
    std::uint64_t _processLastTime = 0;
};
