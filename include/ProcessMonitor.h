#pragma once

#include <string>

class ProcessMonitor
{
public:
    ProcessMonitor(std::wstring ProcessName);

    void Update();

    double TotalProcessTime() const;

    double KernelProcessTime() const;

    double UserProcessTime() const;

    long PageFault() const;

    double UseMemoryMB() const;

private:
    std::wstring _processName;

    double _totalProcessorTime = 0;
    double _kernelProcessorTime = 0;
    double _userProcessorTime = 0;

    long _pageFault = 0;
    double _useMemoryMB = 0;
};
