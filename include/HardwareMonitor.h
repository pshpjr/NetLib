#pragma once

#include "Types.h"


class HardwareMonitor
{
public:
    HardwareMonitor();

    void Update();

    double TotalProcessorTime() const;

    double KernelProcessorTime() const;

    double UserProcessorTime() const;

    long Interrupt() const;

    double InterruptPercent() const;

    long ContextSwitch() const;

    double NonPagedMb() const;

    double AvailableMb() const;

    long HardFault() const;

    double SendKBytes() const;

    double RecvKBytes() const;

    //Monitor items
private:
    double _totalProcessorTime = 0;
    double _kernelProcessorTime = 0;
    double _userProcessorTime = 0;
    long _interrupt = 0;
    double _interruptPercent = 0;
    long _contextSwitch = 0;

    double _nonPagedMb = 0;
    double _availableMb = 0;
    long _hardFault = 0;

    double _sendKBytes = 0;
    double _recvKBytes = 0;
};
