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

    if (state_.load() == State::Stopped)
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
            {
                std::unique_ptr<Executable> owned(executable);
                ExecuteNoThrow(*owned, error, transferred);
            }

            CompleteWork();
        }
        else
        {
            ExecuteNoThrow(*executable, error, transferred);
        }
    }

    std::println("WorkerThread End");
}

bool WindowsExecutor::Stop()
{
    State expected = State::Running;
    if (!state_.compare_exchange_strong(expected, State::Stopping))
        return false;

    if (refCount_.load() == 0)
        TryConfirmStop();

    return true;
}

void WindowsExecutor::Wait()
{
    //  1. 종료 대기
    auto state = state_.load();
    while (state != State::Stopped)
    {
        state_.wait(state);
        state = state_.load();
    }

    //  2. 모든 워커가 Run을 빠져나갈 때까지 대기
    auto count = runningThreadCounts_.load();
    while (count != 0)
    {
        runningThreadCounts_.wait(count);
        count = runningThreadCounts_.load();
    }
}

bool WindowsExecutor::Post(std::function<void()> func)
{
    auto executable = std::make_unique<Executable>(
        Executable::IoType::Custom,
         [func = std::move(func)](NetError, DWORD)
           {
               func();
           });

    if (!TryAddWork())
        return false;

    if (!PostQueuedCompletionStatus(iocpHandle_, 0, 0, executable->GetOverlapped()))
    {
        CompleteWork();
        return false;
    }

    //  pqcs 성공했다면 , Run에서 소유권 받아서 release 한다. 
    executable.release();
    return true;
}

NetError WindowsExecutor::RegisterHandle(HANDLE handle)
{
    if (!TryAddWork())
        return NetError::FromNativeCode(NetError::InvalidExecutorCode);

    if (CreateIoCompletionPort(handle, iocpHandle_, 0, 0) == nullptr)
    {
        const auto error = NetError::FromNativeCode(static_cast<int>(GetLastError()));
        CompleteWork();
        return error;
    }

    return {};
}

void WindowsExecutor::UnregisterHandle()
{
    CompleteWork();
}

void WindowsExecutor::PostInternal(Executable& executable) const
{
    //  세션은 executor를 stop 한 후에도 postRelease 같은 걸 iocp에 post 함. 
    //  외부 노출용 post는 stop 이후 안 받지만, 내부적으로는 상관 없이 push 해줘야 함. 
    PostQueuedCompletionStatus(iocpHandle_, 0, 0, executable.GetOverlapped());
}

bool WindowsExecutor::TryAddWork()
{
    refCount_.fetch_add(1);
    if (state_.load() != State::Running)
    {
        CompleteWork();
        return false;
    }
    return true;
}

void WindowsExecutor::CompleteWork()
{
    if (refCount_.fetch_sub(1) == 1 && state_.load() == State::Stopping)
        TryConfirmStop();
}

void WindowsExecutor::TryConfirmStop()
{
    auto expected = State::Stopping;
    if (!state_.compare_exchange_strong(expected, State::Stopped))
        return;

    state_.notify_all();
    PostQueuedCompletionStatus(iocpHandle_, 0, 0, nullptr);
}

void WindowsExecutor::ExecuteNoThrow(Executable& executable, NetError error, DWORD transferred)
{
    try
    {
        executable.Execute(error, transferred);
    }
    catch (const std::exception& e)
    {
        std::println("Executor callback exception: {}", e.what());
    }
    catch (...)
    {
        std::println("Executor callback unknown exception");
    }
}
