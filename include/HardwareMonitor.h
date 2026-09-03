#pragma once
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
};
