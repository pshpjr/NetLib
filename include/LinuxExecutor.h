#pragma once

#include "SocketTypes.h"
#include "Executable.h"
#include "Types.h"

#include <functional>

class Socket;
class LinuxExecutor
{
    //  RegisterHandle / UnregisterHandle / PostInternal은 라이브러리 내부 전용이다.
    friend class Socket;
    friend class IOCP;

public:
    class WorkGuard
    {
    public:
        WorkGuard(LinuxExecutor& executor)
            : executor_(executor)
        {
        }

        ~WorkGuard() = default;

        LinuxExecutor& executor_;
    };

    LinuxExecutor();
    LinuxExecutor(int maxConcurrent);
    ~LinuxExecutor();

    LinuxExecutor(const LinuxExecutor& other) = delete;
    LinuxExecutor(LinuxExecutor&& other) = delete;
    LinuxExecutor& operator=(const LinuxExecutor& other) = delete;
    LinuxExecutor& operator=(LinuxExecutor&& other) = delete;

    //  호출한 스레드를 워커로 쓴다. 종료 후 호출 시 바로 리턴
    void Run();

    //  thread safe함
    bool Stop();

    //  모든 워커가 종료될 때 까지 대기. [주의] 어떤 워커도 Run() 호출하지 않은 상황에도 대기.
    void Wait();

    //  Stop 이후면 false
    bool Post(std::function<void()> func);
private:
    //  ---- 라이브러리 내부 전용 (Socket) ----
    //  핸들을 executor에 연결한다. Stop 이후면 InvalidExecutor 에러를 리턴한다.
    NetError RegisterHandle(psh::NativeHandle handle);
    void UnregisterHandle();

    void PostInternal(Executable& executable) const;

    void CancelIO(const Socket& socket) const;

    //  refCount ++;
    bool TryAddWork();
    //  refCount_ --. 0 되면 아래 함수로 종료
    void CompleteWork();
    //  Stopping -> Stopped 교체 시
    void TryConfirmStop();

    //  콜백 예외를 잡아 워커 루프가 끊기지 않게 한다.
    static void ExecuteNoThrow(Executable& executable);
};
