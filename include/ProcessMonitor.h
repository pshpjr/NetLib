#pragma once

#include "Types.h"

class ProcessMonitor
{
public:
    ProcessMonitor(String ProcessName);

    void Update();

    double TotalProcessTime() const;

    double KernelProcessTime() const;

    double UserProcessTime() const;

    long PageFault() const;

    double UseMemoryMB() const;
};
