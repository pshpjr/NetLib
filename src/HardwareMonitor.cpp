#include "HardwareMonitor.h"

HardwareMonitor::HardwareMonitor()
{
}

double HardwareMonitor::TotalProcessorTime() const
{
    return _totalProcessorTime;
}

double HardwareMonitor::KernelProcessorTime() const
{
    return _kernelProcessorTime;
}

double HardwareMonitor::UserProcessorTime() const
{
    return _userProcessorTime;
}

long HardwareMonitor::Interrupt() const
{
    return _interrupt;
}

double HardwareMonitor::InterruptPercent() const
{
    return _interruptPercent;
}

long HardwareMonitor::ContextSwitch() const
{
    return _contextSwitch;
}

double HardwareMonitor::NonPagedMb() const
{
    return _nonPagedMb;
}

double HardwareMonitor::AvailableMb() const
{
    return _availableMb;
}

long HardwareMonitor::HardFault() const
{
    return _hardFault;
}

double HardwareMonitor::SendKBytes() const
{
    return _sendKBytes;
}

double HardwareMonitor::RecvKBytes() const
{
    return _recvKBytes;
}

void HardwareMonitor::Update()
{
}
