#pragma once

#include "MyWindows.h"
#include "Types.h"

#include <atomic>
#include <functional>

class WindowsExecutor
{
public:
    WindowsExecutor();
    WindowsExecutor(int maxConcurrent);
    ~WindowsExecutor();

    WindowsExecutor(const WindowsExecutor& other) = delete;
    WindowsExecutor(WindowsExecutor&& other) = delete;
    WindowsExecutor& operator=(const WindowsExecutor & other) = delete;
    WindowsExecutor& operator=(WindowsExecutor && other) = delete;

    void Run();

    //  thread safe함
    bool Stop();

    //  모든 워커가 종료될 때 까지 대기
    void Wait();

    //  Stop 이후면 false
    bool Post(std::function<void()> func);

private:

    enum class State
    {
        Running,
        Stopping,
        Stopped
    };

    HANDLE iocpHandle_ = INVALID_HANDLE_VALUE;
    //  Stop이 불리면 Stopping이 된다. 
    std::atomic<State> state_ = State::Running;
    std::atomic<psh::int32> runningThreadCounts_ = 0;
};
