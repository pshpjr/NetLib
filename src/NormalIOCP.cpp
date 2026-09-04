#include "NormalIOCP.h"

#include <CpuUsage.h>
#include <chrono>
#include <format>
#include <MemoryUsage.h>
#include <process.h>

#include "Logger.h"
#include "CoreGlobal.h"
#include "SendBufferImpl.h"
#include "Executables.h"
#include "GroupManager.h"
#include "IOCP.h"
#include "LockFreeStack.h"
#include "SendBuffer.h"
#include "Session.h"
#include "SettingParser.h"

/*****************************/
//			INIT
/*****************************/

IOCP::IOCP(bool server)
{
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        _isRunning = false;
    }

    for (psh::int16 i = MAX_SESSIONS - 1; i >= 0; i--)
    {
        ReleaseFreeIndex(i);
    }
    _groupManager = std::make_unique<GroupManager>(this);
    _this = this;

    for (auto& session : _sessions)
    {
        session.SetOwner(*this);
    }

    if (server)
    {
        Init();
    }
    else
    {
        printf("Client mode. Not Initialized. please call clientInit Func");
    }
}

bool IOCP::Init()
{
    _settingParser->Init();

    _settingParser->GetValue(L"basic.ip", _ip);
    _settingParser->GetValue(L"basic.port", _port);
    _settingParser->GetValue(L"basic.backlog", _backlog);
    _settingParser->GetValue(L"basic.runningThreads", _maxNetThread);
    _settingParser->GetValue(L"basic.workerThreads", _maxWorkerThread);
    _settingParser->GetValue(L"basic.staticKey", _staticKey);

    printf("%ls %d\n", _ip.c_str(), _port);
    return Init(_ip, _port, _backlog, _maxNetThread, _maxWorkerThread, _staticKey);
}

bool IOCP::ClientInit(psh::uint16 maxRunningThread
    , psh::uint16 workerThread
    , char staticKey
    , bool useGroup
    , bool useMonitor
    , bool useTimeout)
{
    _staticKey = staticKey;
    _iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, maxRunningThread);
    if (_iocp == INVALID_HANDLE_VALUE)
    {
        _listenSocket->Close();
        return false;
    }


    for (int i = 0; i < workerThread; ++i)
    {
        auto handle = (HANDLE)_beginthreadex(nullptr, 0, WorkerEntry, _this, 0, nullptr);
        if (handle == INVALID_HANDLE_VALUE)
        {
            _listenSocket->Close();
            return false;
        }

        _threadArray.push_back({handle, GetThreadId(handle)});

        printf("WorkerThread %p\n", handle);
    }

    if (useGroup)
    {
        const auto groupT = (HANDLE)_beginthreadex(nullptr, 0, GroupEntry, _this, 0, nullptr);
        if (groupT == INVALID_HANDLE_VALUE)
        {
            _listenSocket->Close();
            return false;
        }
        _threadArray.push_back({groupT, GetThreadId(groupT)});
        printf("GroupThread %p\n", groupT);
    }

    if (useTimeout)
    {
        const auto timeoutT = (HANDLE)_beginthreadex(nullptr, 0, TimeoutEntry, _this, 0, nullptr);
        if (timeoutT == INVALID_HANDLE_VALUE)
        {
            _listenSocket->Close();
            return false;
        }
        _threadArray.push_back({timeoutT, GetThreadId(timeoutT)});
        printf("TimeoutThread %p\n", timeoutT);
    }


    if (useMonitor)
    {
        const auto monitorT = (HANDLE)_beginthreadex(nullptr, 0, MonitorEntry, _this, 0, nullptr);
        if (monitorT == INVALID_HANDLE_VALUE)
        {
            _listenSocket->Close();
            return false;
        }
        _threadArray.push_back({monitorT, GetThreadId(monitorT)});
        printf("MonitorThread %p\n", monitorT);
    }

    return true;
}


bool IOCP::Init(const String& ip
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

    _threadArray.resize(workerThread);

    _listenSocket->Init();
    if (_listenSocket->IsValid() == false)
    {
        return false;
    }
    _listenSocket->SetLinger(true);
    //_listenSocket->SetNoDelay(true);
    //_listenSocket->SetSendBuffer(0);
    if (_listenSocket->Bind(ip, port) == false)
    {
        _listenSocket->Close();
        return false;
    }

    if (_listenSocket->Listen(backlog) == false)
    {
        _listenSocket->Close();
        return false;
    }

    const auto acceptT = (HANDLE)_beginthreadex(nullptr, 0, AcceptEntry, _this, 0, nullptr);
    if (acceptT == INVALID_HANDLE_VALUE)
    {
        _listenSocket->Close();
        return false;
    }
    _threadArray.push_back({acceptT, GetThreadId(acceptT)});
    printf("AcceptThread %p\n", acceptT);

    if (ClientInit(_maxNetThread, _maxWorkerThread, _staticKey, true, true, true))
    {
        return false;
    }


    OnInit();
    return true;
}

void IOCP::Start()
{
    _isRunning = true;
    _isRunning.notify_all();
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
    _isRunning = false;
    _listenSocket->Close();
    PostQueuedCompletionStatus(_iocp, 0, 0, nullptr);


    for (const auto [h, id] : _threadArray)
    {
        if (id == GetCurrentThreadId())
        {
            continue;
        }
        if (const auto result = WaitForSingleObject(h, INFINITE);
            result != WAIT_OBJECT_0)
        {
            printf("WaitForMultipleObjects failed %d %d\n", result, GetLastError());
        }
    }

    gracefulEnd = true;
    OnEnd();
    gracefulEnd.notify_one();
}

void IOCP::Wait()
{
    gracefulEnd.wait(false);
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
bool IOCP::SendPacket(const SessionID sessionId, SendBuffer& sendBuffer, int type)
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


    // if (session.CanSend())
    // {
    //     session._sendExecute.Clear();
    //     //InterlockedIncrement(&_iocpCompBufferSize);
    //     PostQueuedCompletionStatus(_iocp, static_cast<DWORD>(-1), (ULONG_PTR)&session
    //         , &session._sendExecute._overlapped);
    // }

    session.Release(L"SendPacketRelease");
    return true;
}

bool IOCP::SendPacketBlocking(SessionID sessionId, SendBuffer& sendBuffer, int type)
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


bool IOCP::SendPackets(const SessionID sessionId, std::vector<SendBuffer>& bufArr)
{
    const auto result = FindSession(sessionId, L"SendPacketInc");
    if (result == nullptr)
    {
        return false;
    }
    auto& session = *result;

    int dataSize = 0;
    for (auto& sendBuffer : bufArr)
    {
        auto buffer = sendBuffer.getBuffer();
        ProcessBuffer(session, *buffer);

        dataSize += sendBuffer.Size();

        if (dataSize < 4096)
        {
            continue;
        }

        //session.TrySend();
        //if (session.CanSend())
        //{
        //	session._sendExecute.Clear();
        //	PostQueuedCompletionStatus(_iocp, -1, (ULONG_PTR)&session, &session._sendExecute._overlapped);
        //}
        dataSize = 0;
    }
    //session.TrySend();
    if (session.CanSend())
    {
        session._sendExecute.Clear();
        PostQueuedCompletionStatus(_iocp, static_cast<DWORD>(-1), (ULONG_PTR)&session
            , session._sendExecute.GetOverlapped());
    }

    session.Release(L"SendPacketRelease");
    return true;
}


void NormalIOCP::ProcessBuffer(Session& session, SendBufferImpl& buffer)
{
    buffer.IncreaseRef(L"ProcessBuffInc");
    if (buffer.GetDataSize() == 0)
    {
        __debugbreak();
    }
    session.EnqueueSendData(&buffer);
}

bool IOCP::GetFreeIndex(unsigned short &index)
{
    return _freeIndex->Pop(index);
}

void IOCP::ReleaseFreeIndex(unsigned short index)
{
    _freeIndex->Push(index);
}

WSAResult<SessionID> IOCP::GetClientSession(const String& ip, const Port port)
{
    unsigned short index;
    GetFreeIndex(index);

    Socket s;
    s.Init();

    if (!s.IsValid())
    {
        return s.LastError();
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

    _sessions[index].SetSocket(s);

    SessionID sessionId{};

    //클라이언트 사용이 많지 않고, 계속 재접속 할 거 아니고, 보통 accept 전에 하니까 그냥 풀어둬도 될 것 같음. 
    //인덱스는 안 겹침. 
    sessionId.id = ++g_sessionId;
    sessionId.index = index;

    _sessions[index].IncreaseRef(L"AcceptInc");
    _sessions[index].SetSessionId(sessionId);
    _sessions[index].RegisterIOCP(_iocp);
    _sessions[index].OffReleaseFlag();
    _sessions[index].SetConnect();
    _sessions[index].SetTimeout(0);
    _sessions[index].SetNetSession(_staticKey);
    _sessions[index].RecvNotIncrease();

    InterlockedIncrement16(&_sessionCount);
    return sessionId;
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


void IOCP::SetDisableClickAndClose()
{
    DWORD consoleModePrev;
    const HANDLE handle = GetStdHandle(STD_INPUT_HANDLE);
    GetConsoleMode(handle, &consoleModePrev);
    SetConsoleMode(handle, consoleModePrev & ~ENABLE_QUICK_EDIT_MODE);

    // get the handle to the console
    const HWND consoleWindow = GetConsoleWindow();

    // get handle to the System Menu
    const HMENU systemMenu = GetSystemMenu(consoleWindow, 0);

    // how many items are there?
    const int itemCount = GetMenuItemCount(systemMenu);
    int j = -1;
    const auto buf = static_cast<WCHAR*>(malloc(256 * sizeof(WCHAR)));

    for (int i = 0; i < itemCount; i++)
    {
        // find the one we want
        GetMenuStringW(systemMenu, i, buf, 255, MF_BYPOSITION);
        if (!wcscmp(buf, L"닫기(&C)"))
        {
            j = i;
            break;
        }
    }

    // if found, remove that menu item
    if (j >= 0)
    {
        RemoveMenu(systemMenu, j, MF_BYPOSITION);
    }
    free(buf);
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
    InterlockedDecrement16(&_sessionCount);
    InterlockedIncrement64(&_disconnectCount);

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
    return gracefulEnd.load();
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

void IOCP::PostExecutable(Executable* exe, const ULONG_PTR arg) const
{
    constexpr int transferred = 2;
    PostQueuedCompletionStatus(_iocp, transferred, arg, exe->GetOverlapped());
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
    if (GetAsyncKeyState('D'))
    {
        Stop();
    }
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

psh::int16 IOCP::GetSessions() const
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

psh::uint32 IOCP::GetSegmentTimeout() const
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
        , L"Sessions", GetSessions()
        , L"Accept", GetAcceptCount(), L"AcceptTPS", GetAcceptTps()
        , L"RecvTPS", GetRecvTps(), L"SendTPS", GetSendTps()
        , L"Disconnect", GetDisconnectCount(), L"DisconnectTPS", GetDisconnectPerSec()
        , L"SegmentTimeout", GetSegmentTimeout(), L"Timeout", GetTimeoutCount()
        , L"GSendPool", SendBufferImpl::_pool.GetGPoolSize(), L"GSendPoolEmpty"
        , SendBufferImpl::_pool.GetGPoolEmptyCount()
        //, L"GRecvPool", RecvBuffer::_pool.GetGPoolSize(), L"GRecvPoolEmpty", RecvBuffer::_pool.GetGPoolEmptyCount()
        , L"SendPoolAcq", SendBufferImpl::_pool.GetAcquireCount(), L"SendPoolRel"
        , SendBufferImpl::_pool.GetReleaseCount()
        //, L"RecvPoolAcq", RecvBuffer::_pool.GetAcquireCount(), L"RecvPoolRel", RecvBuffer::_pool.GetReleaseCount()
        //,L"SendPoolGap", CSendBuffer::_pool.GetAcquireCount()- CSendBuffer::_pool.GetReleaseCount(),L"RecvPoolGap", RecvBuffer::_pool.GetAcquireCount() - RecvBuffer::_pool.GetReleaseCount()
    );
}

void IOCP::PrintMonitorString() const
{
    const auto tmpS = GetLibMonitorString();

    wprintf_s(tmpS.c_str());
}


/*****************************
		 ThreadFunc
*****************************/

unsigned __stdcall IOCP::GroupEntry(const LPVOID arg)
{
    const auto iocp = static_cast<IOCP*>(arg);

    iocp->GroupThread(nullptr);
    return 0;
}

unsigned __stdcall IOCP::MonitorEntry(const LPVOID arg)
{
    const auto iocp = static_cast<IOCP*>(arg);

    iocp->MonitorThread(nullptr);
    return 0;
}

unsigned __stdcall IOCP::TimeoutEntry(const LPVOID arg)
{
    const auto iocp = static_cast<IOCP*>(arg);
    iocp->TimeoutThread(nullptr);
    return 0;
}

unsigned __stdcall IOCP::WorkerEntry(const LPVOID arg)
{
    const auto iocp = static_cast<IOCP*>(arg);
    iocp->WorkerThread(nullptr);
    return 0;
}

unsigned __stdcall IOCP::AcceptEntry(const LPVOID arg)
{
    const auto iocp = static_cast<IOCP*>(arg);
    iocp->AcceptThread(nullptr);
    return 0;
}

void IOCP::GroupThread(LPVOID arg)
{
    using namespace std::chrono;

    UNREFERENCED_PARAMETER(arg);

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
            Sleep(static_cast<DWORD>(sleepTime.count()));
        }
    }
    printf("GroupThread End\n");
}

void IOCP::WorkerThread(LPVOID arg)
{
    using namespace std::chrono;

    UNREFERENCED_PARAMETER(arg);

    srand(GetCurrentThreadId());
    while (true)
    {
        DWORD transferred = 0;
        LPOVERLAPPED overlap = nullptr;
        Session* session = nullptr;


        auto ret = GetQueuedCompletionStatus(_iocp, &transferred, (PULONG_PTR)&session, &overlap, INFINITE);

        Executable* overlapped = Executable::GetExecutable(overlap);

        if (transferred == 0 && session == nullptr)
        {
            PostQueuedCompletionStatus(_iocp, 0, 0, nullptr);
            break;
        }

        if (transferred == 0 && ret != 0)
        {
            session->Release(L"ConnectEndRel", 0);
            continue;
        }


        auto errNo = 0;

        if (transferred == 0 && session != nullptr)
        {
            errNo = WSAGetLastError();
            switch (errNo)
            {
            //정상종료됨.  
            case 0:
                session->SetErr(errNo);
                break;
            case ERROR_NETNAME_DELETED:
                session->SetErr(errNo);
                break;
            //ERROR_SEM_TIMEOUT. 장치에서 끊은 경우 (5회 재전송 실패 등..)
            //제로 윈도우가 계속 떴다. 
            case ERROR_SEM_TIMEOUT:
                {
                auto now = steady_clock::now();
                auto recvWait = duration_cast<milliseconds>(now - session->lastRecv);
                auto sendWait = session->_needCheckSendTimeout
                                    ? duration_cast<milliseconds>(now - session->_postSendExecute.lastSend)
                                    : 0ms;

                gLogger->Write(L"ConnectionError", Logger::LogLevel::Error
                    , L"SemaphoreTimeout  sessionID : %d, executableType : %s, sendWait : %d recvWait : %d"
                    , session->GetSessionId(), Executable::GetIoType(*overlapped).c_str(), sendWait, recvWait);
                session->SetErr(errNo);
                InterlockedIncrement(&_tcpSemaphoreTimeout);
                break;
                }

            //WSA_OPERATION_ABORTED cancleIO로 인한 것.
            //서버가 먼저 끊는 상황에서 발생.
            case WSA_OPERATION_ABORTED:
                gLogger->Write(L"ConnectionError", Logger::LogLevel::Debug
                    , L"Operation Aborted sessionID : %d, executableType : %s", session->GetSessionId()
                    , Executable::GetIoType(*overlapped).c_str());
                session->SetErr(errNo);
                break;
            //io 도중에 closeSocket 호출
            //생기면 안 됨. 
            case ERROR_CONNECTION_ABORTED:
                gLogger->Write(L"ConnectionError", Logger::LogLevel::Error
                    , L"Connection Aborted sessionID : %d, executableType : %s", session->GetSessionId()
                    , Executable::GetIoType(*overlapped).c_str());
                session->SetErr(errNo);
                break;
            case ERROR_NOT_FOUND:
                gLogger->Write(L"ConnectionError", Logger::LogLevel::Error
                    , L"Error Not Found sessionID : %d, executableType : %s", session->GetSessionId()
                    , Executable::GetIoType(*overlapped).c_str());
                session->SetErr(errNo);
                break;
                [[unlikely]] default:
                {
                session->SetErr(errNo);
                auto now = steady_clock::now();
                auto recvWait = duration_cast<milliseconds>(now - session->lastRecv);
                auto sendWait = duration_cast<milliseconds>(
                    now - session->_postSendExecute.lastSend);
                __debugbreak();
                }
            }
            session->Release(L"GQCSErrorRelease", errNo);
        }
        else
        {
            overlapped->Execute((ULONG_PTR)session, transferred, this);
        }
    }
    printf("WorkerThread End\n");
}

//iocp 에서 완료 통지가 올 때 처리는 소켓마다 달라진다 

void IOCP::AcceptThread(LPVOID arg)
{
    UNREFERENCED_PARAMETER(arg);
    WaitStart();

    while (_isRunning)
    {
        Socket clientSocket = _listenSocket->Accept();

        if (!clientSocket.IsValid())
        {
            //accept 서버 닫으려고 중단한 경우. 
            if (const auto err = WSAGetLastError();
                err == WSAEINTR)
            {
                break;
            }

            printf("AcceptError %d", WSAGetLastError());
            _acceptErrorCount++;
            __debugbreak();
        }

        if (OnAccept(clientSocket.GetSockAddress()) == false)
        {
            //TODO: 여기서 cancleIO가 의미 없음. 
            clientSocket.CancelIO();
            continue;
        }

        _acceptCount++;


        clientSocket.SetLinger(true);
        _listenSocket->SetNoDelay(true);
        //_listenSocket->SetSendBuffer(0);

        unsigned short sessionIndex;
        if (GetFreeIndex(sessionIndex) == false)
        {
            printf("!!!!!serverIsFull!!!!!\n");
            clientSocket.CancelIO();
            continue;
        }

        SessionID sessionID{};
        sessionID.id = ++g_sessionId;
        sessionID.index = sessionIndex;

        auto& session = _sessions[sessionIndex];
        session.Reset();

        session.IncreaseRef(L"AcceptInc");
        session.SetSessionId(sessionID);
        session.SetSocket(clientSocket);
        session.RegisterIOCP(_iocp);
        session.SetNetSession(_staticKey);
        session.OffReleaseFlag();

        InterlockedExchange8(&session._connect, true);
        OnConnect(session.GetSessionId(), clientSocket.GetSockAddress());
        session.RecvNotIncrease();

        InterlockedIncrement16(&_sessionCount);
    }
    printf("AcceptThreadEnd\n");
}

void IOCP::MonitorThread(LPVOID arg)
{
    using namespace std::chrono;
    UNREFERENCED_PARAMETER(arg);

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
            _recvTps = InterlockedExchange64(&_recvCount, 0);
            _sendTps = InterlockedExchange64(&_sendCount, 0);
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
            Sleep(static_cast<DWORD>(sleepTime.count()));
        }
    }
    printf("MonitorThread End\n");
}

void IOCP::TimeoutThread(LPVOID arg)
{
    using namespace std::chrono;
    UNREFERENCED_PARAMETER(arg);

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
            Sleep(static_cast<DWORD>(sleepTime.count()));
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
            InterlockedAdd64(&_recvCount, 100);
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
            InterlockedAdd64(&_sendCount, 100);
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
    CloseHandle(_iocp);
    for (const auto [h,id] : _threadArray)
    {
        printf("CloseHandle %p\n", h);

        CloseHandle(h);
    }
}


NormalIOCP::NormalIOCP()
    : _listenSocket{std::make_unique<Socket>()},
    _port(0),
_freeIndex(std::make_unique<LockFreeStack<unsigned short>>())
    , _groupManager(nullptr)
    , _settingParser(std::make_unique<SettingParser>())
,_memMonitor(std::make_unique<MemoryUsage>())
,_cpuMonitor(std::make_unique<CpuUsage>())
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

Session * NormalIOCP::FindSession(const SessionID id, const psh::LPCWSTR content)
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
    _isRunning.wait(false);
}
