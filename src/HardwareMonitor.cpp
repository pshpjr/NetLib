#include "HardwareMonitor.h"

HardwareMonitor::HardwareMonitor()
{
}

double HardwareMonitor::TotalProcessorTime() const
{
    return 0;
}

double HardwareMonitor::KernelProcessorTime() const
{
    return 0;
}

double HardwareMonitor::UserProcessorTime() const
{
    return 0;
}

long HardwareMonitor::Interrupt() const
{
    return 0;
}

double HardwareMonitor::InterruptPercent() const
{
    return 0;
}

long HardwareMonitor::ContextSwitch() const
{
    return 0;
}

double HardwareMonitor::NonPagedMb() const
{
    return 0;
}

double HardwareMonitor::AvailableMb() const
{
    return 0;
}

long HardwareMonitor::HardFault() const
{
    return 0;
}

double HardwareMonitor::SendKBytes() const
{
    return 0;
}

double HardwareMonitor::RecvKBytes() const
{
    return 0;
}

void HardwareMonitor::Update()
{
}
