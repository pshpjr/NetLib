#include "LinuxExecutor.h"
#include "ExceptionTrace.h"

#include "CoreGlobal.h"
#include "Logger.h"
#include "Profiler.h"
#include "Socket.h"
#include "Utility.h"

#include <cstdlib>

#include <fmt/base.h>

#include <liburing.h>
#include <sys/eventfd.h>
#include <unistd.h>

namespace
{
    constexpr std::uintptr_t kWakeTag = 0xDEADBEEF;
    //  취소 요청 자체의 CQE. Windows처럼 취소된 IO의 CQE만 콜백으로 넘기고 이건 버린다.
    constexpr std::uintptr_t kCancelTag = 0xCA9CE1;
}

namespace psh::executor::detail
{
IoUring::IoUring()
{
    eventFd_ = eventfd(0, EFD_CLOEXEC);
    if (eventFd_ < 0)
        throw std::runtime_error("Failed to create eventfd");

    if (io_uring_queue_init(1024, &ioUring_, 0) < 0)
    {
        close(eventFd_);
        throw std::runtime_error("Failed to initialize io_uring");
    }

    uringPollThread_ = std::thread([this]
    {
        psh::util::RunWithTrace("io_uring polling thread", [this] { Loop(); });
    });
}

IoUring::~IoUring()
{
    Stop();
    uringPollThread_.join();
    io_uring_queue_exit(&ioUring_);
    close(eventFd_);
}

std::optional<IoUring::IoResult> IoUring::WaitResponse()
{
    std::unique_lock lock(doneMutex_);
    doneCv_.wait(lock, [this]
    {
        return !done_.empty() || stop_;
    });
    if (done_.empty())
        return std::nullopt;

    const auto completion = done_.front();
    done_.pop_front();
    return completion;
}

void IoUring::RequestIo(const io_uring_sqe& sqe)
{
    if (stop_)
        return;

    {
        std::lock_guard lock(pendingMutex_);
        pending_.push_back(sqe);
    }
    RequestWork();
}

void IoUring::CancelIo(const psh::NativeSocket handle)
{
    //  RequestIo와 같은 큐를 거쳐야 먼저 요청된 IO보다 늦게 제출된다.
    io_uring_sqe sqe{};
    io_uring_prep_cancel_fd(&sqe, handle, IORING_ASYNC_CANCEL_ALL);
    io_uring_sqe_set_data64(&sqe, kCancelTag);
    RequestIo(sqe);
}

void IoUring::Stop()
{
    if (stop_.exchange(true))
        return;

    RequestWork();
}

void IoUring::RegisterRead()
{
    auto* sqe = GetRingSqe();
    io_uring_prep_read(sqe, eventFd_, &wakeValue_, sizeof(wakeValue_), 0);
    io_uring_sqe_set_data(sqe, reinterpret_cast<void*>(static_cast<std::uintptr_t>(kWakeTag)));
    auto ret = io_uring_submit(&ioUring_);
    if (ret < 0)
        //  read 제출 실패는 치명적 에러로 처리
        throw std::runtime_error("Failed to submit io_uring read request");
}

io_uring_sqe* IoUring::GetRingSqe()
{
    auto* sqe = io_uring_get_sqe(&ioUring_);
    //  제출 링이 가득 차면 먼저 제출해서 비운다.
    if (sqe == nullptr)
    {
        io_uring_submit(&ioUring_);
        sqe = io_uring_get_sqe(&ioUring_);
        if (sqe == nullptr)
            std::abort();
    }
    return sqe;
}

void IoUring::FlushPendingRequest()
{
    {
        std::lock_guard lock(pendingMutex_);
        working_.swap(pending_);
    }
    if (working_.empty())
        return;

    for (const auto& src : working_)
        std::memcpy(GetRingSqe(), &src, sizeof(src));
    working_.clear();

    while (io_uring_sq_ready(&ioUring_) != 0)
    {
        const auto ret = io_uring_submit(&ioUring_);
        if (ret <= 0)
            throw std::runtime_error("Failed to submit io_uring requests");
    }
}

void IoUring::Loop()
{
    RegisterRead();

    //  cqe를 수집해서 한 번에 done_에 넣기 위한 임시벡터
    std::vector<IoResult> ioResults;

    while (true)
    {
        PrepareWait();

        if (stop_)
            break;

        WaitCompletion();

        if (auto containReadCompletion = CollectCompletions(ioResults))
            RegisterRead();

        PublishResults(ioResults);
        ioResults.clear();
    }

    //  stop_ 설정과 notify 사이에 doneMutex_를 거쳐야 pred 검사 후 wait 진입 전인 워커가 신호를 놓치지 않는다.
    {
        std::lock_guard lock(doneMutex_);
    }
    doneCv_.notify_all();
}

void IoUring::PrepareWait()
{
    while (true)
    {
        FlushPendingRequest();

        auto expected = State::Running;
        if (state_.compare_exchange_strong(expected, State::Sleeping))
            return;

        state_.store(State::Running);
    }
}

void IoUring::WaitCompletion()
{
    io_uring_cqe* cqe = nullptr;
    const auto ret = io_uring_wait_cqe(&ioUring_, &cqe);

    state_.store(State::Running);

    if (ret < 0 && ret != -EINTR)
        util::DebugBreak();
}

bool IoUring::CollectCompletions(std::vector<IoResult>& outCompletions)
{
    io_uring_cqe* cqe;
    unsigned head;
    unsigned count = 0;
    bool containWakeup = false;

    io_uring_for_each_cqe(&ioUring_, head, cqe)
    {
        ++count;

        if (cqe->user_data == kWakeTag)
        {
            containWakeup = true;
            continue;
        }

        if (cqe->user_data == kCancelTag)
            continue;

        outCompletions.push_back({io_uring_cqe_get_data(cqe), cqe->res, cqe->flags});
    }

    io_uring_cq_advance(&ioUring_, count);
    return containWakeup;
}

void IoUring::PublishResults(const std::vector<IoResult>& ioResults)
{
    if (ioResults.empty())
        return;

    const auto count = ioResults.size();

    {
        std::lock_guard lock(doneMutex_);
        done_.insert(done_.end(), ioResults.begin(), ioResults.end());
    }

    for (std::size_t i = 0; i < count; ++i)
        doneCv_.notify_one();
}

void IoUring::RequestWork()
{
    auto state = state_.load();
    while (true)
    {
        if (state == State::RunningDirty)
            return;

        //  잠들어 있으면 running으로 바꾸고 깨운다. running이면 dirty만 표시한다.
        const auto next = state == State::Sleeping ? State::Running : State::RunningDirty;
        if (state_.compare_exchange_weak(state, next))
        {
            if (next == State::Running)
            {
                //  파일에 써서 깨우기
                const std::uint64_t buf = 1;
                if (write(eventFd_, &buf, sizeof(buf)) != sizeof(buf))
                    util::DebugBreak();
            }
            return;
        }
    }
}
}

LinuxExecutor::LinuxExecutor() = default;

LinuxExecutor::LinuxExecutor(int maxConcurrent)
{
}

LinuxExecutor::~LinuxExecutor() = default;

void LinuxExecutor::Run()
{
    runningThreadCounts_.fetch_add(1);

    SCOPE_GUARD
    {
        if (runningThreadCounts_.fetch_sub(1) == 1)
            runningThreadCounts_.notify_all();
    };

    if (state_.load() == State::Stopped)
        return;

    psh::util::SetThreadName("IOCP Worker Thread");
    PRO_THREAD("WORKER");
    srand(psh::util::GetCurrentThreadId());

    while (true)
    {
        auto ret = ioUring_.WaitResponse();
        //  오류: CQE 대기 실패
        if (!ret.has_value())
        {
            fmt::println("Critical: Failed to wait for CQE: " );
            break;
        }

        const auto error = ret->res >= 0 ? NetError{} : NetError::FromNativeCode(-ret->res);
        const auto transferred = ret->res >= 0 ? static_cast<std::size_t>(ret->res) : 0;

        auto* executable = static_cast<Executable*>(ret->userData);
        if (executable == nullptr)
        {
            io_uring_sqe sqe{};
            io_uring_prep_nop(&sqe);
            ioUring_.RequestIo(sqe);
            break;
        }

        //  즉시 실패한 경우 result가 이미 있다.
        if (!executable->HasResult())
        {
            executable->SetResult(error, transferred);
        }

        //  Custom 타입은 Post에서 만들고 Run에서 Release 한다.
        if (executable->GetIoType() == Executable::IoType::Custom)
        {
            {
                std::unique_ptr<Executable> owned(executable);
                ExecuteNoThrow(*owned);
            }
            CompleteWork();
        }
        else
        {
            ExecuteNoThrow(*executable);
        }
    }

    fmt::println("WorkerThread End");
}

bool LinuxExecutor::Stop()
{
    auto expected = State::Running;
    if (!state_.compare_exchange_strong(expected, State::Stopping))
        return false;

    if (refCount_.load() == 0)
        TryConfirmStop();

    return true;
}

void LinuxExecutor::Wait()
{
    auto state = state_.load();
    while (state != State::Stopped)
    {
        state_.wait(state);
        state = state_.load();
    }

    auto count = runningThreadCounts_.load();
    while (count != 0)
    {
        runningThreadCounts_.wait(count);
        count = runningThreadCounts_.load();
    }
}

bool LinuxExecutor::Post(std::function<void()> func)
{
    auto executable = std::make_unique<Executable>(
        Executable::IoType::Custom,
        [func = std::move(func)](NetError, std::size_t)
        {
            func();
        });

    if (!TryAddWork())
        return false;

    io_uring_sqe sqe{};
    io_uring_prep_nop(&sqe);
    sqe.user_data = reinterpret_cast<std::uint64_t>(executable.get());
    ioUring_.RequestIo(sqe);

    executable.release();

    return true;
}

NetError LinuxExecutor::RegisterHandle(psh::NativeHandle handle)
{
    if (!TryAddWork())
        return NetError::FromNativeCode(NetError::InvalidExecutorCode);

    return NetError{};
}

void LinuxExecutor::UnregisterHandle()
{
    CompleteWork();
}


void LinuxExecutor::PostInternal(Executable& executable)
{
    io_uring_sqe sqe{};
    io_uring_prep_nop(&sqe);
    sqe.user_data = reinterpret_cast<std::uint64_t>(&executable);
    ioUring_.RequestIo(sqe);
}

void LinuxExecutor::CancelIO(const Socket& socket)
{
    ioUring_.CancelIo(socket.GetNativeHandle());
}

bool LinuxExecutor::TryAddWork()
{
    refCount_.fetch_add(1);
    if (state_.load() != State::Running)
    {
        CompleteWork();
        return false;
    }
    return true;
}




void LinuxExecutor::CompleteWork()
{
    if (refCount_.fetch_sub(1) == 1 && state_.load() == State::Stopping)
        TryConfirmStop();
}

void LinuxExecutor::TryConfirmStop()
{
    auto expected = State::Stopping;
    if (!state_.compare_exchange_strong(expected, State::Stopped))
        return;

    state_.notify_all();
    io_uring_sqe sqe{};
    io_uring_prep_nop(&sqe);
    ioUring_.RequestIo(sqe);
}


void LinuxExecutor::ExecuteNoThrow(Executable& executable)
{
    try
    {
        psh::util::RunWithTrace("LinuxExecutor::ExecuteNoThrow", [&]
        {
            executable.Execute();
        });
    }
    catch (const std::exception& e)
    {
        fmt::println("Executor callback exception: {}", e.what());
    }
    catch (...)
    {
        fmt::println("Executor callback unknown exception");
    }
}
