#pragma once

#include "BuildOption.h"
#include "GroupManager.h"
#include "SessionTypes.h"

#include IOCP_HEADER

class SendBuffer;
class RecvBuffer;
class Executable;
class Client;


template <typename T>
class SingleThreadQ;

class IOCP : public IOCP_CLASS
{
    friend class RecvExecutable;
    friend class ReleaseExecutable;
    friend class PostSendExecutable;
    friend class Session;
    friend class Group;
    friend class GroupManager;
    friend class Client;

public:
    IOCP(bool server = true);

    bool Init();
    bool ClientInit(psh::uint16 maxRunningThread, psh::uint16 workerThread, char staticKey, bool useGroup, bool useMonitor,bool useTimeout);
    bool Init(const String& ip
              , Port port
              , psh::uint16 backlog
              , psh::uint16 maxRunningThread
              , psh::uint16 workerThread
              , char staticKey);
    void Start();
    void Stop();
    void Wait();
    bool SendPacket(SessionID sessionId, SendBuffer& sendBuffer, int type = 0);
    bool SendPacketBlocking(SessionID sessionId, SendBuffer& sendBuffer, int type = 0);
    //deprecate
    bool SendPackets(SessionID sessionId, Vector<SendBuffer>& bufArr);

    bool DisconnectSession(SessionID sessionId);
    bool isEnd() const;
    void SetMaxPacketSize(int size);
    void SetTimeout(SessionID sessionId, int timeoutMillisecond);
    void SetDefaultTimeout(unsigned int timeoutMillisecond);
    void PostExecutable(Executable* exe, ULONG_PTR arg) const;

    void SetSessionStaticKey(SessionID id, char staticKey);
    void HandleInput();

    bool GetFreeIndex(unsigned short &index);

    void ReleaseFreeIndex(unsigned short index);

    //connect
    WSAResult<SessionID> GetClientSession(const String& ip, Port port);
    bool IsValidSession(SessionID id);
    //GROUP


    //CONTENT VIRTUAL

    virtual void OnWorkerThreadBegin()
    {
    }

    virtual void OnWorkerThreadEnd()
    {
    }

    virtual bool OnAccept(SockAddr_in)
    {
        return true;
    }

    virtual void OnConnect(SessionID sessionId, const SockAddr_in& info)
    {
        UNREFERENCED_PARAMETER(sessionId);
        UNREFERENCED_PARAMETER(info);
    }

    virtual void OnDisconnect(SessionID sessionId, int wsaErrCode)
    {
        UNREFERENCED_PARAMETER(sessionId);
    }

    virtual void OnRecvPacket(SessionID sessionId, RecvBuffer& buffer)
    {
        UNREFERENCED_PARAMETER(sessionId);
        UNREFERENCED_PARAMETER(buffer);
    }

    virtual void OnInit()
    {
    }

    virtual void OnStart()
    {
    }

    virtual void OnEnd()
    {
    }

    virtual void OnSessionTimeout(SessionID sessionId, timeoutInfo info)
    {
        UNREFERENCED_PARAMETER(sessionId);
        UNREFERENCED_PARAMETER(info);
    }

    virtual void OnMonitorRun()
    {
    }

    //MONITOR
    psh::uint64 GetAcceptCount() const;
    psh::uint64 GetAcceptTps() const;
    psh::uint64 GetRecvTps() const;
    psh::uint64 GetSendTps() const;
    psh::int16 GetSessions() const;
    psh::uint64 GetPacketPoolSize() const;
    psh::uint32 GetPacketPoolEmptyCount() const;
    psh::uint64 GetTimeoutCount() const;
    static psh::uint32 GetPacketPoolAcquireCount();
    static psh::uint32 GetPacketPoolReleaseCount();
    psh::uint64 GetDisconnectCount() const;
    psh::uint64 GetDisconnectPerSec() const;
    psh::uint32 GetSegmentTimeout() const;

    size_t GetPageFaultCount() const;
    size_t GetPeakPMemSize() const;
    size_t GetPMemSize() const;
    size_t GetPeakVirtualMemSize() const;
    size_t GetVirtualMemSize() const;

    String GetLibMonitorString() const;
    void PrintMonitorString() const;


    template <typename GroupType, typename... Args>
    GroupID CreateGroup(Args&&... args) const;
    void MoveSession(SessionID target, GroupID dst) const;
    bool CheckPostRelease(SessionID id, GroupID groupId);

private:
    void GroupSessionDisconnect(Session* session);
    void postReleaseSession(SessionID id);

    //MONITOR
    void IncreaseRecvCount(int value);
    void increaseSendCount(int value);

    void _onDisconnect(SessionID sessionId, int wsaErrCode);
    void onRecvPacket(const Session& session, RecvBuffer& buffer);

    //WorkerThreadFunc
    void GroupThread(LPVOID arg);
    void WorkerThread(LPVOID arg);
    void AcceptThread(LPVOID arg);
    void MonitorThread(LPVOID arg);
    void TimeoutThread(LPVOID arg);


    static unsigned __stdcall GroupEntry(LPVOID arg);
    static unsigned __stdcall WorkerEntry(LPVOID arg);
    static unsigned __stdcall AcceptEntry(LPVOID arg);

    static unsigned __stdcall MonitorEntry(LPVOID arg);
    static unsigned __stdcall TimeoutEntry(LPVOID arg);
};


template <typename GroupType, typename... Args>
GroupID IOCP::CreateGroup(Args&&... args) const
{
    return _groupManager->CreateGroup<GroupType>(std::forward<Args>(args)...);
}
