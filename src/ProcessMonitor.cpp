#include "ProcessMonitor.h"

ProcessMonitor::ProcessMonitor(String)
{
}

double ProcessMonitor::TotalProcessTime() const
{
    return 0;
}

double ProcessMonitor::KernelProcessTime() const
{
    return 0;
}

double ProcessMonitor::UserProcessTime() const
{
    return 0;
}

long ProcessMonitor::PageFault() const
{
    return 0;
}

double ProcessMonitor::UseMemoryMB() const
{
    return 0;
}

void ProcessMonitor::Update()
{
}
