#pragma once
#include <atomic>
#include <memory>
#include <optional>
#include <vector>

#include "SessionTypes.h"
#include "LockFreeStack.h"
#include "Session.h"

class GroupManager;
class MemoryUsage;
class CpuUsage;
class Socket;
class Session;
class SettingParser;
class SendBufferImpl;


class threadMonitor
{
public:
    threadMonitor(const HANDLE hThread) : _hThread(hThread)
    {
    }

    void Update()
    {
        FILETIME ftCreation, ftExit, ftKernel, ftUser;
        GetThreadTimes(_hThread, &ftCreation, &ftExit, &ftKernel, &ftUser);

        const ULARGE_INTEGER ulKernel = FileTimeToLargeInteger(ftKernel);
        const ULARGE_INTEGER ulUser = FileTimeToLargeInteger(ftUser);

        const ULONGLONG kernelTime = ulKernel.QuadPart / 10000;
        const ULONGLONG userTime = ulUser.QuadPart / 10000;

        if (_lastKernelTime > 0)
        {
            _kernelTime = kernelTime - _lastKernelTime;
            _lastKernelTime = kernelTime;
        }

        if (_lastUserTime > 0)
        {
            _userTime = userTime - _lastUserTime;
            _lastUserTime = userTime;
        }
    }

    ULONGLONG KernelTime() const
    {
        return _kernelTime;
    }

    ULONGLONG UserTime() const
    {
        return _userTime;
    }

    ULONGLONG TotTime() const
    {
        return KernelTime() + UserTime();
    }

private:
    ULARGE_INTEGER FileTimeToLargeInteger(const FILETIME& ft)
    {
        ULARGE_INTEGER ul{};
        ul.HighPart = ft.dwHighDateTime;
        ul.LowPart = ft.dwLowDateTime;
        return ul;
    }

private:
    HANDLE _hThread;
    ULONGLONG _lastKernelTime = 0;
    ULONGLONG _lastUserTime = 0;

    ULONGLONG _kernelTime = 0;
    ULONGLONG _userTime = 0;
};

class NormalIOCP
{
public:
    NormalIOCP(const NormalIOCP& other) = delete;
    NormalIOCP(NormalIOCP&& other) = delete;
    NormalIOCP& operator=(const NormalIOCP& other) = delete;
    NormalIOCP& operator=(NormalIOCP&& other) = delete;

protected:
    NormalIOCP();

    inline std::optional<Session*> findSession(SessionID id, psh::LPCWSTR content = L"");

    Session* FindSession(const SessionID id, const psh::LPCWSTR content = L"");

    static unsigned short GetSessionIndex(const SessionID sessionID);

    static inline void ProcessBuffer(Session& session, SendBufferImpl& buffer);
    void WaitStart();

protected:
    static constexpr int MAX_SESSIONS = 15000;
    static constexpr long releaseFlag = 0x0010'0000;

    virtual ~NormalIOCP();


    /*
    *
    *	IOCP
    *
    */

    HANDLE _iocp = INVALID_HANDLE_VALUE;
    std::unique_ptr<Socket> _listenSocket;

    std::vector<std::pair<HANDLE, DWORD>> _threadArray;

    String _ip{};
    psh::uint16 _maxNetThread = 0;
    psh::uint16 _maxWorkerThread = 0;
    psh::uint16 _port{};
    psh::uint16 _backlog = 0;

    bool _checkTiemout = true;
    void* _this = nullptr;
    char _staticKey = 0;
    std::atomic_bool gracefulEnd = false;
    std::atomic_bool _isRunning = false;

    /*
    *
    *	MONITOR
    *
    */
    psh::uint64 _acceptCount = 0;
    psh::uint64 _oldAccepCount = 0;
    std::atomic<psh::int64> _recvCount = 0;
    std::atomic<psh::int64> _sendCount = 0;
    psh::uint64 _oldDisconnect = 0;
    std::atomic<psh::int64> _disconnectCount = 0;
    std::atomic<psh::uint32> _tcpSemaphoreTimeout = 0;
    psh::uint64 _acceptTps = 0;
    psh::uint64 _recvTps = 0;
    psh::uint64 _sendTps = 0;
    psh::uint64 _disconnectps = 0;
    std::atomic<short> _sessionCount = 0;
    psh::uint64 _packetPoolSize = 0;
    psh::uint32 _packetPoolEmpty = 0;
    psh::uint64 _timeoutSessions = 0;
    psh::uint32 _acceptErrorCount = 0;
    long _iocpCompBufferSize = 0;

    //std::vector<threadMonitor> _threadMonitors;
    std::unique_ptr<MemoryUsage> _memMonitor;
    std::unique_ptr<CpuUsage>  _cpuMonitor;

    /*
    *
    *	SESSION_MANAGER
    *
    */
    std::array<Session,MAX_SESSIONS> _sessions;
    std::unique_ptr<LockFreeStack<unsigned short>>  _freeIndex;
    psh::uint64 g_sessionId = 0;

    /*
    *
    *	GROUP_MANAGER
    *
    */

    std::unique_ptr<GroupManager> _groupManager;
    std::unique_ptr<SettingParser> _settingParser;
};
