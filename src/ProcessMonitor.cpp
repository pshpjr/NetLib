#include "ProcessMonitor.h"

ProcessMonitor::ProcessMonitor(std::wstring ProcessName)
    : _processName(ProcessName)
{
}

double ProcessMonitor::TotalProcessTime() const
{
    return _totalProcessorTime;
}

double ProcessMonitor::KernelProcessTime() const
{
    return _kernelProcessorTime;
}

double ProcessMonitor::UserProcessTime() const
{
    return _userProcessorTime;
}

long ProcessMonitor::PageFault() const
{
    return _pageFault;
}

double ProcessMonitor::UseMemoryMB() const
{
    return _useMemoryMB;
}

void ProcessMonitor::Update()
{
}
