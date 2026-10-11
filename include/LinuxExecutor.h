#pragma once

#include "SocketTypes.h"
#include "Executable.h"
#include "Types.h"

#include <atomic>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <functional>
#include <liburing.h>
#include <thread>

class Socket;

namespace psh::executor::detail
{
    class IoUring
    {
    public:
        struct IoResult
        {
            void* userData = nullptr;
            //  성공이면 처리한 바이트 수, 실패면 -errno
            std::int32_t res = 0;
            std::uint32_t flags = 0;
        };

        IoUring();

        ~IoUring();

        std::optional<IoResult> WaitResponse();

        void RequestIo(const io_uring_sqe& sqe);

        //  handle에 걸린 IO를 전부 취소한다. 취소된 IO는 -ECANCELED로 완료되고, 취소 요청 자체의 CQE는 버린다.
        void CancelIo(psh::NativeSocket handle);

        void Stop();

    private:
        enum class State
        {
            Sleeping,
            Running,
            RunningDirty
        };

        void Loop();
        void RegisterRead();
        void PrepareWait();
        void FlushPendingRequest();
        void WaitCompletion();
        bool CollectCompletions(std::vector<IoResult>& ioResults);
        void PublishResults(const std::vector<IoResult>& ioResults);
        void RequestWork();

        io_uring_sqe* GetRingSqe();

        io_uring ioUring_{};
        std::thread uringPollThread_{};

        //  내부 스레드 깨우는 용도
        psh::NativeHandle eventFd_{};
        //  내부 스레드 깨울 때 write하는 메모리 공간
        std::uint64_t wakeValue_ = 0;

        std::atomic<State> state_{State::Sleeping};
        std::atomic_bool stop_{};

        //  pending: 외부 요청 모음.
        std::mutex pendingMutex_;
        std::vector<io_uring_sqe> pending_;
        //  락 잡는 구간 줄이기 위해 pending과 스왑함. 메모리 할당 줄이기 위해 재사용하려고 멤버로 뺌
        std::vector<io_uring_sqe> working_;

        //  완료된 IO 모음.
        std::mutex doneMutex_;
        std::condition_variable doneCv_;
        std::deque<IoResult> done_;
    };
}


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
            executor_.refCount_.fetch_add(1);
        }

        ~WorkGuard()
        {
            executor_.CompleteWork();
        }

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
    enum class State
    {
        Running,
        Stopping,
        Stopped
    };


    //  ---- 라이브러리 내부 전용 (Socket) ----
    NetError RegisterHandle(psh::NativeHandle handle);
    void UnregisterHandle();

    void PostInternal(Executable& executable);

    void CancelIO(const Socket& socket);

    /*
     * socket의 모든 에러 콜백이 정상적으로 호출되어야 session 정리가 가능해진다.
     * 정리 전에 모든 워커가 루프를 벗어나지 못하도록 대기 중인 작업 및 소켓의 수를 카운팅한다.
     * 레퍼런스를 마지막으로 정리한 워커가 종료를 트리거해서 다른 워커들이 종료할 수 있도록 한다.
     */

    bool TryAddWork();
    void CompleteWork();
    void TryConfirmStop();

    //  콜백 예외를 잡아 워커 루프가 끊기지 않게 한다.
    static void ExecuteNoThrow(Executable& executable);

    psh::executor::detail::IoUring ioUring_ = {};

    std::atomic<State> state_ = State::Running;
    std::atomic<psh::int32> runningThreadCounts_ = 0;

    alignas(64) std::atomic<psh::int64> refCount_ = 0;


    std::mutex queueMutex_ = {};
    std::deque<Executable> queue_;


};
