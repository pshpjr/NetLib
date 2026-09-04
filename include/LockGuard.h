#pragma once
#include <shared_mutex>

class ReadLockGuard
{
public:
    ReadLockGuard(const ReadLockGuard& other) = delete;
    ReadLockGuard(ReadLockGuard&& other) = delete;
    ReadLockGuard& operator=(const ReadLockGuard& other) = delete;
    ReadLockGuard& operator=(ReadLockGuard&& other) = delete;

    explicit ReadLockGuard(std::shared_mutex& lock)
        : _cs(lock)
    {
        _cs.lock_shared();
    }

    ~ReadLockGuard()
    {
        _cs.unlock_shared();
    }

private:
    std::shared_mutex& _cs;
};

class WriteLockGuard
{
public:
    WriteLockGuard(const WriteLockGuard& other) = delete;
    WriteLockGuard(WriteLockGuard&& other) = delete;
    WriteLockGuard& operator=(const WriteLockGuard& other) = delete;
    WriteLockGuard& operator=(WriteLockGuard&& other) = delete;

    explicit WriteLockGuard(std::shared_mutex& lock)
        : _cs(lock)
    {
        _cs.lock();
    }

    ~WriteLockGuard()
    {
        _cs.unlock();
    }

private:
    std::shared_mutex& _cs;
};
