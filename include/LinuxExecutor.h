#pragma once
#include "SocketTypes.h"
#include "Executable.h"

#include <functional>

class LinuxExecutor
{
public:
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

    //  refCount ++;
    bool TryAddWork();
    //  refCount_ --. 0 되면 아래 함수로 종료
    void CompleteWork();
    //  Stopping -> Stopped 교체 시
    void TryConfirmStop();

    //  콜백 예외를 잡아 워커 루프가 끊기지 않게 한다.
    static void ExecuteNoThrow(Executable& executable);

    HANDLE iocpHandle_ = INVALID_HANDLE_VALUE;

    //    읽기 위주 변수

    //  Stop이 불리면 Stopping이 된다.
    std::atomic<State> state_ = State::Running;
    std::atomic<psh::int32> runningThreadCounts_ = 0;

    //    쓰기 위주 변수
    alignas(64) std::atomic<psh::int64> refCount_ = 0;
};
