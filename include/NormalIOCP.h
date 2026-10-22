#pragma once
#include "LockFreeStack.h"
#include "Metrics.h"
#include "Session.h"
#include "SessionTypes.h"
#include "ThreadWrapper.h"

#include <atomic>
#include <memory>
#include <optional>
#include <thread>
#include <vector>

class GroupManager;
class MemoryUsage;
class CpuUsage;
class Socket;
class Session;
class SendBufferImpl;


#ifdef _WIN32
#include "MyWindows.h"

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
    HANDLE _hThread;
    ULONGLONG _lastKernelTime = 0;
    ULONGLONG _lastUserTime = 0;

    ULONGLONG _kernelTime = 0;
    ULONGLONG _userTime = 0;
};

#endif



class NormalIOCP
{
protected:
    static constexpr int MAX_SESSIONS = 15000;
    static constexpr long releaseFlag = 0x0010'0000;

public:
    NormalIOCP(const NormalIOCP& other) = delete;
    NormalIOCP(NormalIOCP&& other) = delete;
    NormalIOCP& operator=(const NormalIOCP& other) = delete;
    NormalIOCP& operator=(NormalIOCP&& other) = delete;

protected:
    NormalIOCP();


    virtual ~NormalIOCP();

    inline std::optional<Session*> findSession(SessionID id, psh::LPCWSTR content = L"");

    Session* FindSession(const SessionID id, const psh::LPCWSTR content = L"");

    static unsigned short GetSessionIndex(const SessionID sessionID);

    static inline void ProcessBuffer(Session& session, SendBufferImpl& buffer);
    void WaitStart();


    /*
    *
    *	IOCP
    *
    */

    //  _listenSocket보다 먼저 선언되어야 한다. (멤버 초기화 순서)
    Executor _executor;
    std::unique_ptr<Socket> _listenSocket;

    std::vector<psh::ThreadWrapper> _threadArray;
    /*TODO: 타입들 전부 내 타입으로 수정하기
     */
    Ip _ip{};
    psh::uint16 _maxNetThread = 0;
    psh::uint16 _maxWorkerThread = 0;
    Port _port{};
    psh::uint16 _backlog = 0;

    bool _checkTiemout = true;
    char _staticKey = 0;
    std::atomic_bool gracefulEnd = false;
    //  Stop 전까지 유지되는 1 + 실행 중인 스레드 수
    std::atomic<psh::int32> _aliveRef = 1;
    std::atomic_bool _isRunning = false;
    std::atomic_bool _startSignal = false;

    /*
    *
    *	MONITOR
    *
    */
    std::atomic<short> _sessionCount = 0;
    psh::uint32 _acceptErrorCount = 0;
    long _iocpCompBufferSize = 0;

    //vector<threadMonitor> _threadMonitors;
    std::unique_ptr<MemoryUsage> _memMonitor;
    std::unique_ptr<CpuUsage> _cpuMonitor;

    //  _sessionMetric 콜백이 _sessionCount를 읽으므로 그보다 뒤에 선언한다.
    psh::metrics::Counter _acceptMetric;
    psh::metrics::Counter _disconnectMetric;
    psh::metrics::Counter _recvPacketMetric;
    psh::metrics::Counter _sendPacketMetric;
    psh::metrics::Counter _timeoutMetric;
    psh::metrics::Counter _semaphoreTimeoutMetric;
    psh::metrics::CallbackGauge _sessionMetric;

    //  다른 스레드에서 읽으면 안 되는 값이라 MonitorThread가 1초마다 Set한다.
    psh::metrics::Gauge _virtualMemMetric;
    psh::metrics::Gauge _peakVirtualMemMetric;
    psh::metrics::Gauge _physicalMemMetric;
    psh::metrics::Gauge _peakPhysicalMemMetric;
    psh::metrics::DoubleGauge _processCpuMetric;
    psh::metrics::DoubleGauge _processKernelCpuMetric;
    psh::metrics::DoubleGauge _processUserCpuMetric;
    psh::metrics::DoubleGauge _systemCpuMetric;
    psh::metrics::DoubleGauge _systemKernelCpuMetric;
    psh::metrics::DoubleGauge _systemUserCpuMetric;
    psh::metrics::Gauge _sendPoolMetric;
    psh::metrics::Gauge _sendPoolEmptyMetric;
    psh::metrics::Gauge _sendPoolAcquireMetric;
    psh::metrics::Gauge _sendPoolReleaseMetric;

    /*
    *
    *	SESSION_MANAGER
    *
    */
    std::array<Session, MAX_SESSIONS> _sessions;
    std::unique_ptr<LockFreeStack<unsigned short>> _freeIndex;
    psh::uint64 g_sessionId = 0;

    /*
    *
    *	GROUP_MANAGER
    *
    */

    std::unique_ptr<GroupManager> _groupManager;
};
