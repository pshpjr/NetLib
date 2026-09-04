//
// Created by pshpjr on 26. 9. 4..
//

#ifndef GAMESERVER_THREAD_MONITOR_H
#define GAMESERVER_THREAD_MONITOR_H
#include <thread>

#include "Types.h"

//  리눅스에선 다른 스레드의 시간 update 못 함. Update 호출하는 스레드는 생성자에서 넣은 스레드와 같아야 함. 
class ThreadMonitor
{
public:
    explicit ThreadMonitor(std::thread& thread) : _thread(thread)
    {
    }

    void Update();

    psh::uint64 KernelTimeMs() const
    {
        return _kernelTimeMs;
    }

    psh::uint64 UserTimeMs() const
    {
        return _userTimeMs;
    }

    psh::uint64 TotalTimeMs() const
    {
        return KernelTimeMs() + UserTimeMs();
    }

private:
    std::thread& _thread;
    bool _hasPreviousSample = false;
    psh::uint64 _lastKernelTimeMs = 0;
    psh::uint64 _lastUserTimeMs = 0;

    psh::uint64 _kernelTimeMs = 0;
    psh::uint64 _userTimeMs = 0;
};


#endif //GAMESERVER_THREAD_MONITOR_H
