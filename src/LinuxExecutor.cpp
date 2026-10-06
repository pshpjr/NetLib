#include "LinuxExecutor.h"

#include "Socket.h"

LinuxExecutor::LinuxExecutor() = default;

LinuxExecutor::LinuxExecutor(int /*maxConcurrent*/)
{
}

LinuxExecutor::~LinuxExecutor() = default;

void LinuxExecutor::Run()
{
}

bool LinuxExecutor::Stop()
{
    return false;
}

void LinuxExecutor::Wait()
{
}

bool LinuxExecutor::Post(std::function<void()> /*func*/)
{
    return false;
}

NetError LinuxExecutor::RegisterHandle(psh::NativeHandle /*handle*/)
{
    return NetError::FromNativeCode(ENOSYS);
}

void LinuxExecutor::UnregisterHandle()
{
}

void LinuxExecutor::PostInternal(Executable& /*executable*/) const
{
}

void LinuxExecutor::CancelIO(const Socket& socket) const
{
}

bool LinuxExecutor::TryAddWork()
{
    return false;
}

void LinuxExecutor::CompleteWork()
{
}

void LinuxExecutor::TryConfirmStop()
{
}

void LinuxExecutor::ExecuteNoThrow(Executable& /*executable*/)
{
}
