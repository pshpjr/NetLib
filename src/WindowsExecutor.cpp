#include "WindowsExecutor.h"

#include "Executable.h"
#include "Profiler.h"
#include "SocketTypes.h"
#include "Utility.h"

#include <print>
#include <stdexcept>

WindowsExecutor::WindowsExecutor() : WindowsExecutor(0)
{

}

WindowsExecutor::WindowsExecutor(int maxConcurrent)
{
    iocpHandle_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, maxConcurrent);
    if (iocpHandle_ == nullptr)
    {
        throw std::runtime_error("Failed to create IOCP handle");
    }
}

WindowsExecutor::~WindowsExecutor()
{
    HANDLE oldHandle = std::exchange(iocpHandle_, INVALID_HANDLE_VALUE);
    if (oldHandle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(oldHandle);
    }
}

void WindowsExecutor::Run()
{
    runningThreadCounts_.fetch_add(1);

    // 함수 리턴 시 count sub
    SCOPE_GUARD
    {
        if (runningThreadCounts_.fetch_sub(1) == 1)
            runningThreadCounts_.notify_all();
    };

    if (state_.load() != State::Running)
        return;

    psh::util::set_thread_name("IOCP Worker Thread");
    PRO_THREAD("WORKER");
    srand(GetCurrentThreadId());

    while (true)
    {
        DWORD transferred = 0;
        ULONG_PTR key = 0;
        OVERLAPPED* overlap = nullptr;

        const auto ret = GetQueuedCompletionStatus(iocpHandle_, &transferred, &key, &overlap, INFINITE);

        //  1. 종료 처리
        if (transferred == 0 && overlap == nullptr)
        {
            PostQueuedCompletionStatus(iocpHandle_, 0, 0, nullptr);
            break;
        }

        NetError error{};

        if (transferred == 0 && ret == 0)
        {
            error = NetError::FromNativeCode(WSAGetLastError());
        }

        auto executable = Executable::GetExecutable(overlap);
        //  Custom 타입은 Post에서 만들고 Run에서 Release 한다. 
        if (executable->GetIoType() == Executable::IoType::Custom)
        {
            std::unique_ptr<Executable> owned(executable);
            owned->Execute(error, transferred);
        }
        else
        {
            executable->Execute(error, transferred);
        }
    }

    std::println("WorkerThread End");
}

bool WindowsExecutor::Stop()
{
    State expected = State::Running;
    if (!state_.compare_exchange_strong(expected, State::Stopping))
        return false;

    PostQueuedCompletionStatus(iocpHandle_, 0, 0, nullptr);
    return true;
}

void WindowsExecutor::Wait()
{
    auto count = runningThreadCounts_.load();
    while (count != 0)
    {
        runningThreadCounts_.wait(count);
        count = runningThreadCounts_.load();
    }

    state_.store(State::Stopped);
}

bool WindowsExecutor::Post(std::function<void()> func)
{
    if (state_.load() != State::Running)
        return false;

    auto executable = std::make_unique<Executable>(
        Executable::IoType::Custom,
         [func = std::move(func)](NetError, DWORD)
           {
               func();
           });

    if (!PostQueuedCompletionStatus(iocpHandle_, 0, 0, executable->GetOverlapped()))
        return false;

    //  pqcs 성공했다면 , Run()에서 소유권 받아서 release 한다. 
    executable.release();
    return true;
}
