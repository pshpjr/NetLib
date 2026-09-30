#pragma once

#include "MyWindows.h"
#include "SocketTypes.h"
#include "Types.h"

#include <atomic>
#include <functional>

class Executable;

class WindowsExecutor
{
    //  RegisterHandle / UnregisterHandle / PostInternal은 라이브러리 내부 전용이다.
    friend class Socket;
    friend class IOCP;

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

    //  ---- 라이브러리 내부 전용 (Socket) ----
    //  핸들을 executor에 연결한다. Stop 이후면 InvalidExecutor 에러를 리턴한다.
    NetError RegisterHandle(HANDLE handle);
    void UnregisterHandle();

    void PostInternal(Executable& executable) const;

    //  콜백 예외를 잡아 워커 루프가 끊기지 않게 한다.
    static void ExecuteNoThrow(Executable& executable, NetError error, DWORD transferred);

    HANDLE iocpHandle_ = INVALID_HANDLE_VALUE;
    //  Stop이 불리면 Stopping이 된다. 
    std::atomic<State> state_ = State::Running;
    std::atomic<psh::int32> runningThreadCounts_ = 0;
};
