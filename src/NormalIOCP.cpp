#include "NormalIOCP.h"
#include "Utility.h"

#include <chrono>
#include <format>
#include <iostream>
#include <thread>
#include <fmt/base.h>


#include "CpuUsage.h"
#include "MemoryUsage.h"
#include "Logger.h"
#include "CoreGlobal.h"
#include "SendBufferImpl.h"
#include "Executable.h"
#include "GroupManager.h"
#include "IOCP.h"
#include "LockFreeStack.h"
#include "Profiler.h"
#include "SendBuffer.h"
#include "Session.h"

/*****************************/
//			INIT
/*****************************/

IOCP::IOCP()
{
	if (Socket::InitPlatform().HasError())
	{
		_isRunning = false;
        throw std::runtime_error("Failed to initialize platform");
	}

	for (psh::int16 i = MAX_SESSIONS - 1; i >= 0; i--)
	{
		ReleaseFreeIndex(i);
	}
	_groupManager = std::make_unique<GroupManager>(this);

	for (auto& session : _sessions)
	{
		session.SetOwner(*this);
	}
}

bool IOCP::ClientInit(psh::uint16 maxRunningThread
                      , psh::uint16 workerThread
                      , char staticKey
                      , bool useGroup
                      , bool useMonitor
                      , bool useTimeout)
{
    _staticKey = staticKey;
    //  TODO: maxRunningThread(IOCP 동시 실행 스레드 수)는 executor로 옮기면서 지금은 쓰이지 않는다.
    static_cast<void>(maxRunningThread);


    for (int i = 0; i < workerThread; ++i)
    {
        _threadArray.emplace_back(&IOCP::WorkerThread, this);
    }

    if (useGroup)
    {
        _threadArray.emplace_back(&IOCP::GroupThread, this);
    }

    if (useTimeout)
    {
        _threadArray.emplace_back(&IOCP::TimeoutThread, this);
    }


    if (useMonitor)
    {
        _threadArray.emplace_back(&IOCP::MonitorThread, this);
    }

    return true;
}


bool IOCP::Init(Ip ip
                , const Port port
                , const psh::uint16 backlog
                , const psh::uint16 maxRunningThread
                , const psh::uint16 workerThread
                , const char staticKey)
{
    _maxWorkerThread = workerThread;
    _backlog = backlog;
    _maxNetThread = maxRunningThread;
    _staticKey = staticKey;
    _ip = ip;
    _port = port;

    _threadArray.reserve(workerThread + 4);

    if (_listenSocket->Init().HasError())
    {
        return false;
    }
    _listenSocket->SetLinger(true);
    //_listenSocket->SetNoDelay(true);
    //_listenSocket->SetSendBuffer(0);
    if (_listenSocket->Bind(ip, port).HasError())
    {
        _listenSocket->Close();
        return false;
    }

    if (_listenSocket->Listen(backlog).HasError())
    {
        _listenSocket->Close();
        return false;
    }

    _threadArray.emplace_back(&IOCP::AcceptThread, this);

    if (!ClientInit(_maxNetThread, _maxWorkerThread, _staticKey, true, true, true))
    {
        return false;
    }


    OnInit();
    return true;
}

void IOCP::Start()
{
    _isRunning.store(true);
    _startSignal.store(true);
    _startSignal.notify_all();
    OnStart();
}

/*
워커에서 스레드를 멈추고자 한다면?
stop이 OnEnd 호출해도 종료가 안 된 상황임.
stop을 호출하는 순간 멈춰야 하는가?
ㄴㄴ 안 그래도 됨.

워커에서 호출하면?
이 함수를 호출하고 나서 다른 스레드들이 종료된 후
함수를 나가고, 뭔가 작업을 더 할지도 모름.

원격으로 서버를 끄고 싶은 경우.
패킷 종류에 따라 recv 한 후 워커가 이 함수를 호출할거임.
그리고 나서 처리해야 할 또 다른 패킷들이 있을 수 있음.
stop을 쏘고 패킷을 또 보내는 멍청한 짓을 하면 그것도 처리함.
아니면 stop을 호출하면
*/
//void DissociateThreadFromIoCompletionPort()
//{
//	auto iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1);
//	DWORD bytes;
//	ULONG_PTR key;
//	LPOVERLAPPED overlapped;
//	GetQueuedCompletionStatus(iocp, &bytes, &key, &overlapped, 0);
//	CloseHandle(iocp);
//}

void IOCP::Stop()
{
    if (!_isRunning.exchange(false))
    {
        return;
    }
    _startSignal.store(true);
    _startSignal.notify_all();
    _listenSocket->StopListening();
    _listenSocket->Close();

    //  신규 Post와 소켓 등록을 먼저 막는다. 이후 accept된 소켓은 등록이 거부된다.
    _executor.Stop();

    //  TODO: SessionManager.closeAll()로 이동. 모든 세션을 끊어야 등록된 소켓이 해제되어 워커가 종료된다.
    for (auto& session : _sessions)
    {
        DisconnectSession(session.GetSessionId());
    }

    gracefulEnd.store(true);
    gracefulEnd.notify_all();
}

void IOCP::Wait()
{
    gracefulEnd.wait(false);

    for (auto& thread : _threadArray)
    {
        if (thread.joinable())
            thread.join();
    }
    OnEnd();
}

void IOCP::SetTimeout(const SessionID sessionId, const int timeoutMillisecond)
{
    const auto result = FindSession(sessionId, L"TimeoutInc");
    if (result == nullptr)
    {
        return;
    }
    auto& session = *result;

    session.SetTimeout(timeoutMillisecond);

    session.Release(L"setTimeoutRel");
}

void IOCP::SetDefaultTimeout(const unsigned int timeoutMillisecond)
{
    if (timeoutMillisecond == 0)
    {
        _checkTiemout = false;
        return;
    }

    for (auto& s : _sessions)
    {
        s.SetDefaultTimeout(timeoutMillisecond);
        s.SetTimeout(timeoutMillisecond);
    }
}

/*****************************/
//			Network
/*****************************/
bool IOCP::SendPacket(const SessionID sessionId, const SendBuffer& sendBuffer, int type)
{
    PRO_FUNCTION();

    const auto result = FindSession(sessionId, L"SendPacketInc");
    if (result == nullptr)
        return false;

    auto& session = *result;
    session.WriteContentLog(type);

    ProcessBuffer(session, *sendBuffer.getBuffer());

    /*TODO: 성능 비교하기.
     * 지금 버전이 반응성 더 낮고, CPU는 조금 덜 쓰리라 예상
     */
    session.TrySend();


    // if (session.TryAcquireSendFlag())
    // {

    //     session._sendExecute.Clear();
    //     //InterlockedIncrement(&_iocpCompBufferSize);
    //     PostQueuedCompletionStatus(_iocp, static_cast<DWORD>(-1), (ULONG_PTR)&session
    //         , session._sendExecute.GetOverlapped());
    // }

    session.Release(L"SendPacketRelease");
    return true;
}

bool IOCP::SendPacketDirect(SessionID sessionId, SendBuffer& sendBuffer, int type)
{
    auto buffer = sendBuffer.getBuffer();
    const auto result = FindSession(sessionId, L"SendPacketInc");
    if (result == nullptr)
    {
        return false;
    }

    auto& session = *result;

    session.WriteContentLog(type);

    ProcessBuffer(session, *buffer);
    session.TrySend();
    session.Release(L"SendPacketRelease");
    return true;
}


//TODO: 지금 함수 정상 작동 안 할거임

// bool IOCP::SendPackets(const SessionID sessionId, std::vector<SendBuffer>& bufArr)
// {
//     const auto result = FindSession(sessionId, L"SendPacketInc");
//     if (result == nullptr)
//     {
//         return false;
//     }
//     auto& session = *result;

//     int dataSize = 0;
//     for (auto& sendBuffer : bufArr)
//     {
//         auto buffer = sendBuffer.getBuffer();
//         ProcessBuffer(session, *buffer);

//         dataSize += sendBuffer.Size();

//         if (dataSize < 4096)
//         {
//             continue;
//         }

//         //session.TrySend();
//         //if (session.TryAcquireSendFlag())
//         //{
//         //	session._sendExecute.Clear();
//         //	PostQueuedCompletionStatus(_iocp, -1, (ULONG_PTR)&session, &session._sendExecute._overlapped);
//         //}
//         dataSize = 0;
//     }
//     //session.TrySend();
//     if (session.TryAcquireSendFlag())
//     {
//         session._sendExecute.Clear();
//         PostQueuedCompletionStatus(_iocp, static_cast<DWORD>(-1), (ULONG_PTR)&session
//                                    , session._sendExecute.GetOverlapped());
//     }

//     session.Release(L"SendPacketRelease");
//     return true;
// }


void IOCP::ProcessBuffer(Session& session, SendBufferImpl& buffer)
{
    PRO_FUNCTION();
    buffer.IncreaseRef(L"ProcessBuffInc");
    if (buffer.GetDataSize() == 0)
    {
        psh::util::DebugBreak();
    }
    //버퍼가 가득 찰 정도면 끊는다.
    if (session.EnqueueSendData(&buffer) == false)
    {
        buffer.Release(L"Session SendBuffer full");
        DisconnectSession(session.GetSessionId());
    }
}

bool IOCP::GetFreeIndex(unsigned short& index)
{
    return _freeIndex->Pop(index);
}

void IOCP::ReleaseFreeIndex(unsigned short index)
{
    _freeIndex->Push(index);
}

psh::Result<SessionID, NetError> IOCP::GetClientSession(Ip ip, const Port port)
{
    unsigned short index;
    GetFreeIndex(index);

    Socket s(_executor);
    if (const auto initResult = s.Init(); initResult.HasError())
    {
        ReleaseFreeIndex(index);
        return initResult.Error();
    }

    s.SetLinger(true);
    s.SetNoDelay(true);

    if (auto connectionResult = s.Connect(ip, port);
        !connectionResult.HasValue())
    {
        gLogger->Write(L"ClientClose", Logger::LogLevel::Debug, L"connect fail");
        s.Close();
        ReleaseFreeIndex(index);

        return connectionResult.Error();
    }

    auto& session = InitSession(index, std::move(s));
    session.SetTimeout(0);
    session.RecvNotIncrease();

    ++_sessionCount;
    return session.GetSessionId();
}

Session& IOCP::InitSession(const unsigned short index, Socket&& socket)
{
    // accept 스레드와 GetClientSession이 동시에 올릴 수 있다. 클라이언트 연결은 드물고 보통 accept 전에 하며,
    // index가 겹치지 않으므로 id 부분이 중복돼도 SessionID는 구분된다.
    SessionID sessionId{};
    sessionId.id = ++g_sessionId;
    sessionId.index = index;

    // Reset이 이전 소켓을 닫고 타임아웃을 기본값으로 되돌리므로 소켓·타임아웃 설정보다 먼저 한다.
    auto& session = _sessions[index];
    session.Reset();

    session.IncreaseRef(L"AcceptInc");
    session.SetSessionId(sessionId);
    session.SetSocket(std::move(socket));
    session.SetNetSession(_staticKey);
    session.OffReleaseFlag();
    session.SetConnect();
    return session;
}

bool IOCP::IsValidSession(const SessionID id)
{
    const auto findResult = FindSession(id);
    if (findResult != nullptr)
    {
        findResult->Release(L"validSessionRelease");
    }

    return nullptr != findResult;
}


bool IOCP::DisconnectSession(const SessionID sessionId)
{
    const auto result = FindSession(sessionId, L"DisconnectInc");
    if (result == nullptr)
    {
        return false;
    }

    auto& session = *result;

    session.Close();

    session.Release(L"DisconnectRel");

    return true;
}

void IOCP::_onDisconnect(const SessionID sessionId, int wsaErrCode)
{
    --_sessionCount;
    ++_disconnectCount;

    OnDisconnect(sessionId, wsaErrCode);
}

void IOCP::onRecvPacket(const Session& session, RecvBuffer& buffer)
{
    try
    {
        OnRecvPacket(session._sessionId, buffer);
    }
    catch (const std::invalid_argument& err)
    {
        auto info = err.what();
        gLogger->Write(L"RecvErr", Logger::LogLevel::Debug, L"Can't pop : recv buffer is empty");
        DisconnectSession(session._sessionId);
    }
}

/*****************************/
//			INIT
/*****************************/

bool IOCP::isEnd() const
{
    return gracefulEnd;
}

void IOCP::SetMaxPacketSize(const int size)
{
    for (auto& session : _sessions)
    {
        session.SetMaxPacketLen(size);
    }
}


/*****************************
			ThreadPool
*****************************/

void IOCP::PostExecutable(Executable& exe)
{
    _executor.PostInternal(exe);
}

void IOCP::SetSessionStaticKey(const SessionID id, const char staticKey)
{
    const auto result = FindSession(id, L"setStaticKey");
    if (result == nullptr)
    {
        return;
    }
    auto& session = *result;

    session.SetNetSession(staticKey);
    session.Release();
}

void IOCP::HandleInput()
{

}


/*****************************
			Monitor
*****************************/

psh::uint64 IOCP::GetAcceptCount() const
{
    return _acceptCount;
}

psh::uint64 IOCP::GetAcceptTps() const
{
    return _acceptTps;
}

psh::uint64 IOCP::GetRecvTps() const
{
    return _recvTps;
}

psh::uint64 IOCP::GetSendTps() const
{
    return _sendTps;
}

psh::int16 IOCP::GetSessionCount() const
{
    return _sessionCount;
}

psh::uint64 IOCP::GetPacketPoolSize() const
{
    return _packetPoolSize;
}

psh::uint32 IOCP::GetPacketPoolEmptyCount() const
{
    return _packetPoolEmpty;
}

psh::uint64 IOCP::GetTimeoutCount() const
{
    return _timeoutSessions;
}

psh::uint32 IOCP::GetPacketPoolAcquireCount()
{
    return SendBufferImpl::_pool.GetAcquireCount();
}

psh::uint32 IOCP::GetPacketPoolReleaseCount()
{
    return SendBufferImpl::_pool.GetReleaseCount();
}

psh::uint64 IOCP::GetDisconnectCount() const
{
    return _disconnectCount;
}

psh::uint64 IOCP::GetDisconnectPerSec() const
{
    return _disconnectps;
}

psh::uint32 IOCP::GetSemaphoreTimeoutCount() const
{
    return _tcpSemaphoreTimeout;
}

size_t IOCP::GetPageFaultCount() const
{
    return _memMonitor->GetPageFaultCount();
}

size_t IOCP::GetPeakPMemSize() const
{
    return _memMonitor->GetPeakPMemSize();
}

size_t IOCP::GetPMemSize() const
{
    return _memMonitor->GetPMemSize();
}

size_t IOCP::GetPeakVirtualMemSize() const
{
    return _memMonitor->GetPeakVirtualMemSize();
}

size_t IOCP::GetVirtualMemSize() const
{
    return _memMonitor->GetVirtualMemSize();
}

size_t IOCP::GetPeakPagedPoolUsage() const
{
    return _memMonitor->GetPeakPagedPoolUsage();
}

size_t IOCP::GetPeakNonPagedPoolUsage() const
{
    return _memMonitor->GetPeakNonPagedPoolUsage();
}

size_t IOCP::GetPagedPoolUsage() const
{
    return _memMonitor->GetPagedPoolUsage();
}

size_t IOCP::GetNonPagedPoolUsage() const
{
    return _memMonitor->GetNonPagedPoolUsage();
}

String IOCP::GetLibMonitorString() const
{
    return std::format(L"==================================================================================\n"
                       L" {:<11s}{:^55s}{:>11s}\n"
                       L"----------------------------------------------------------------------------------\n"
                       L"+  {:<14s} : {:>6.2f}MB / {:<6.2f}MB | {:<14s} : {:>6.2f}MB / {:<6.2f}MB \n"
                       L"+  {:<14s} : {:>6.2f}%% / {:<6.2f}%%  |\n"
                       L"+  {:<14s} : {:>6.2f}%% / {:<6.2f}%%  |  {:<14s} : {:>6.2f}%% / {:<6.2f}%%  \n"
                       L"----------------------------------------------------------------------------------\n"
                       L"+  {:<14s} : {:>10d}\n"
                       L"+  {:<14s} : {:>10d}, {:<14s} : {:>10d}\n"
                       L"+  {:<14s} : {:>10d}, {:<14s} : {:>10d}\n"
                       L"+  {:<14s} : {:>10d}, {:<14s} : {:>10d}\n"
                       L"+  {:<14s} : {:>10d}, {:<14s} : {:>10d}\n"
                       L"+  {:<14s} : {:>10d}, {:<14s} : {:>10d}\n"
                       //L"+  {:<14s} : {:>10d}, {:<14s} : {:>10d}\n"
                       L"+  {:<14s} : {:>10d}, {:<14s} : {:>10d}\n"
                       //L"+  {:<14s} : {:>10d}, {:<14s} : {:>10d}\n"
                       //L"+  {:<14s} : {:>10d}, {:<14s} : {:>10d}\n"
                       L"----------------------------------------------------------------------------------\n"
                       , L"+++++", L"LIBS", L"+++++"
                       , L"VirtualMem", GetVirtualMemSize() / 1000'000., GetPeakVirtualMemSize() / 1000'000.
                       , L"PhysicalMem", GetPMemSize() / 1000'000., GetPeakPMemSize() / 1000'000.
                       , L"TotalCPU", _cpuMonitor->ProcessTotal(), _cpuMonitor->ProcessorTotal()
                       , L"KernelCPU", _cpuMonitor->ProcessKernel(), _cpuMonitor->ProcessorKernel()
                       , L"UserCPU", _cpuMonitor->ProcessUser(), _cpuMonitor->ProcessorUser()
                       , L"Sessions", GetSessionCount()
                       , L"Accept", GetAcceptCount(), L"AcceptTPS", GetAcceptTps()
                       , L"RecvTPS", GetRecvTps(), L"SendTPS", GetSendTps()
                       , L"Disconnect", GetDisconnectCount(), L"DisconnectTPS", GetDisconnectPerSec()
                       , L"SegmentTimeout", GetSemaphoreTimeoutCount(), L"Timeout", GetTimeoutCount()
                       , L"GSendPool", SendBufferImpl::_pool.GetGPoolSize(), L"GSendPoolEmpty"
                       , SendBufferImpl::_pool.GetGPoolEmptyCount()
                       //, L"GRecvPool", RecvBuffer::_pool.GetGPoolSize(), L"GRecvPoolEmpty", RecvBuffer::_pool.GetGPoolEmptyCount()
                       , L"SendPoolAcq", SendBufferImpl::_pool.GetAcquireCount(), L"SendPoolRel"
                       , SendBufferImpl::_pool.GetReleaseCount()
                       //, L"RecvPoolAcq", RecvBuffer::_pool.GetAcquireCount(), L"RecvPoolRel", RecvBuffer::_pool.GetReleaseCount()
                       //,L"SendPoolGap", SendBufferImpl::_pool.GetAcquireCount()- SendBufferImpl::_pool.GetReleaseCount(),L"RecvPoolGap", RecvBuffer::_pool.GetAcquireCount() - RecvBuffer::_pool.GetReleaseCount()
    );
}

void IOCP::PrintMonitorString() const
{
    fmt::print("{}", psh::util::WToS(GetLibMonitorString()));
}


/*****************************
		 ThreadFunc
*****************************/





//  TODO: executor에 타이머 기능 추가하면 제거
void IOCP::GroupThread()
{
    psh::util::SetThreadName("IOCP Group Thread");
    using namespace std::chrono;


    WaitStart();
    const auto start = steady_clock::now();
    auto next = time_point_cast<milliseconds>(start) + milliseconds(1);
    while (_isRunning)
    {
        _groupManager->Update();

        auto sleepTime = duration_cast<milliseconds>(next - steady_clock::now());
        next += milliseconds(1);
        if (sleepTime.count() > 0)
        {
            std::this_thread::sleep_for(sleepTime);
        }
    }
    printf("GroupThread End\n");
}

void IOCP::WorkerThread()
{
    _executor.Run();
}

void IOCP::AcceptThread()
{
    psh::util::SetThreadName("IOCP Accept Thread");
    WaitStart();

    while (_isRunning)
    {
        auto acceptResult = _listenSocket->Accept();

        //  서버 중단하려고 accept 중단한 경우
        if (!_isRunning.load())
            break;

        if (acceptResult.HasError())
        {
            //accept 서버 닫으려고 중단한 경우.
            const auto error = acceptResult.Error();
            //  executor가 Stop되어 소켓 등록이 거부된 경우도 종료로 본다.
            if (error == NetErrc::Interrupted || error == NetErrc::InvalidExecutor)
            {
                break;
            }

            printf("AcceptError %d", error.GetNativeCode());
            _acceptErrorCount++;
            psh::util::DebugBreak();
            continue;
        }

        Socket clientSocket = std::move(acceptResult.Value());
        if (OnAccept(clientSocket.GetSockAddress()) == false)
        {
            //TODO: 여기서 cancleIO가 의미 없음.
            clientSocket.CancelIO();
            continue;
        }

        _acceptCount++;


        clientSocket.SetLinger(true);
        clientSocket.SetNoDelay(true);
        //_listenSocket->SetSendBuffer(0);

        unsigned short sessionIndex;
        if (GetFreeIndex(sessionIndex) == false)
        {
            printf("!!!!!serverIsFull!!!!!\n");
            clientSocket.CancelIO();
            continue;
        }

        auto& session = InitSession(sessionIndex, std::move(clientSocket));
        OnConnect(session.GetSessionId(), session.GetAddress());
        session.RecvNotIncrease();

        ++_sessionCount;
    }
    printf("AcceptThreadEnd\n");
}

void IOCP::MonitorThread()
{
    psh::util::SetThreadName("IOCP Monitor Thread");
    using namespace std::chrono;

    WaitStart();
    const auto start = steady_clock::now();
    auto next = start + 1s;
    while (_isRunning)
    {
        {
            _acceptTps = _acceptCount - _oldAccepCount;
            _oldAccepCount = _acceptCount;
            _disconnectps = _disconnectCount - _oldDisconnect;
            _oldDisconnect = _disconnectCount;
            _recvTps = _recvCount.exchange(0);
            _sendTps = _sendCount.exchange(0);
            _packetPoolSize = SendBufferImpl::_pool.GetGPoolSize();
            _packetPoolEmpty = SendBufferImpl::_pool.GetGPoolEmptyCount();

            _memMonitor->Update();
            _cpuMonitor->UpdateCpuTime();

            OnMonitorRun();
        }


        auto sleepTime = duration_cast<milliseconds>(next - steady_clock::now());
        next += 1s;
        if (sleepTime.count() > 0)
        {
            std::this_thread::sleep_for(sleepTime);
        }
    }
    printf("MonitorThread End\n");
}

void IOCP::TimeoutThread()
{
    psh::util::SetThreadName("IOCP Timeout Thread");
    using namespace std::chrono;

    WaitStart();
    const auto start = steady_clock::now();
    auto next = time_point_cast<milliseconds>(start) + milliseconds(1000);
    while (_isRunning)
    {
        const auto now = steady_clock::now();

        if (_checkTiemout)
        {
            for (auto& session : _sessions)
            {
                if (auto result = session.CheckTimeout(now))
                {
                    OnSessionTimeout(session.GetSessionId(), result.value());
                    session.ResetTimeoutWait();
                    _timeoutSessions++;
                }
            }
        }
        auto sleepTime = duration_cast<milliseconds>(next - steady_clock::now());
        next += milliseconds(1000);
        if (sleepTime.count() > 0)
        {
            std::this_thread::sleep_for(sleepTime);
        }
    }
    printf("TimeoutThread End\n");
}

void IOCP::IncreaseRecvCount(const int value)
{
    thread_local int localRecvCount;

    for (int i = 0; i < value; i++)
    {
        ++localRecvCount;
        if (localRecvCount == 100)
        {
            _recvCount.fetch_add(100);
            localRecvCount = 0;
        }
    }
}

void IOCP::increaseSendCount(const int value)
{
    thread_local int localSendCount;
    for (int i = 0; i < value; i++)
    {
        ++localSendCount;
        if (localSendCount == 100)
        {
            _sendCount.fetch_add(100);
            localSendCount = 0;
        }
    }
}

void IOCP::MoveSession(const SessionID target, const GroupID dst) const
{
    _groupManager->MoveSession(target, dst);
}

bool IOCP::CheckPostRelease(SessionID id, GroupID groupId)
{
    auto& session = _sessions[id.index];
    if (session.GetGroupID() == groupId && session.GetSessionId() == id && session.GetRefCount(id) >= releaseFlag)
    {
        session.PostRelease();
    }
#ifdef SESSION_DEBUG
    session.Write(-1, GroupID(0), L"Normal Group Leave");
#endif //SESSION_DEBUG


    return true;
}

void IOCP::GroupSessionDisconnect(Session* session)
{
    _groupManager->SessionDisconnected(session);
}

void IOCP::postReleaseSession(SessionID id)
{
    auto& session = _sessions[id.index];
    session.PostRelease();
}


NormalIOCP::~NormalIOCP()
{
    _isRunning.store(false);
    _startSignal.store(true);
    _startSignal.notify_all();
    _listenSocket->Close();
    _listenSocket->StopListening();
    _executor.Stop();
    for (auto& thread : _threadArray)
    {
        if (thread.joinable()) thread.join();
    }
    Socket::CleanupPlatform();
}


NormalIOCP::NormalIOCP()
    : _listenSocket{std::make_unique<Socket>(_executor)}
      , _port(0)
      , _freeIndex(std::make_unique<LockFreeStack<unsigned short>>())
      , _groupManager(nullptr)
      , _memMonitor(std::make_unique<MemoryUsage>())
      , _cpuMonitor(std::make_unique<CpuUsage>())
{
}

std::optional<Session*> NormalIOCP::findSession(const SessionID id, const psh::LPCWSTR content)
{
    auto& session = _sessions[id.index];
    const auto result = session.IncreaseRef(content);

    //릴리즈 중이거나 세션 변경되었으면
    if (result > releaseFlag || session.GetSessionId().id != id.id)
    {
        //세션 릴리즈 해도 문제 없음. 플레그 서 있을거라 내가 올린 만큼 내려감.
        session.Release(L"sessionChangedRelease");
        return {};
    }

    return &session;
}

Session* NormalIOCP::FindSession(const SessionID id, const psh::LPCWSTR content)
{
    if (id.index >= MAX_SESSIONS || id.index < 0)
    {
        return nullptr;
    }

    auto& session = _sessions[id.index];

    //릴리즈 중이거나 세션 변경되었으면
    if (session.IncreaseRef(content) > releaseFlag || session.GetSessionId().id != id.id)
    {
        //세션 릴리즈 해도 문제 없음. 플레그 서 있을거라 내가 올린 만큼 내려감.
        session.Release(L"sessionChangedRelease");
        return nullptr;
    }

    return &session;
}

unsigned short NormalIOCP::GetSessionIndex(const SessionID sessionID)
{
    return sessionID.index;
};

void NormalIOCP::WaitStart()
{
    _startSignal.wait(false);
}
