#pragma once

#include "BuildOption.h"
#include "GroupManager.h"
#include "SessionTypes.h"
#include "Executor.h"
#include "Executable.h"

#include <functional>

#include IOCP_HEADER

class SendBuffer;
class RecvBuffer;
class Client;


template <typename T>
class SingleThreadQ;

/*TODO: 성능 비교하기.
 * 지금 버전이 반응성 더 낮고, CPU는 조금 덜 쓰리라 예상
 */
class IOCP : public IOCP_CLASS
{
    friend class Session;
    friend class Group;
    friend class GroupManager;
    friend class Client;

public:
    IOCP();

    bool Init();
    bool ClientInit(psh::uint16 maxRunningThread, psh::uint16 workerThread, char staticKey, bool useGroup
                    , bool useMonitor, bool useTimeout);
    bool Init(const String& ip
              , Port port
              , psh::uint16 backlog
              , psh::uint16 maxRunningThread
              , psh::uint16 workerThread
              , char staticKey);
    void Start();
    void Stop();
    void Wait();
    bool SendPacket(SessionID sessionId, const SendBuffer& sendBuffer, int type = 0);
    bool SendPacketDirect(SessionID sessionId, SendBuffer& sendBuffer, int type = 0);
    // [[deprecated]]
    // bool SendPackets(SessionID sessionId, Vector<SendBuffer>& bufArr);

    bool DisconnectSession(SessionID sessionId);
    bool isEnd() const;
    void SetMaxPacketSize(int size);
    void SetTimeout(SessionID sessionId, int timeoutMillisecond);
    void SetDefaultTimeout(unsigned int timeoutMillisecond);
    void PostExecutable(Executable& exe) const;

    void SetSessionStaticKey(SessionID id, char staticKey);
    void HandleInput();

    bool GetFreeIndex(unsigned short& index);

    void ReleaseFreeIndex(unsigned short index);
    void ProcessBuffer(Session& session, SendBufferImpl& buffer);
    //connect
    psh::Result<SessionID, NetError> GetClientSession(const String& ip, Port port);
    bool IsValidSession(SessionID id);
    //GROUP


    //CONTENT VIRTUAL

    virtual void OnWorkerThreadBegin()
    {
    }

    virtual void OnWorkerThreadEnd()
    {
    }

    virtual bool OnAccept(SockAddr)
    {
        return true;
    }

    virtual void OnConnect(SessionID sessionId, const SockAddr& info)
    {
    }

    virtual void OnDisconnect(SessionID sessionId, int wsaErrCode)
    {
    }

    virtual void OnRecvPacket(SessionID sessionId, RecvBuffer& buffer)
    {
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

    //세션이 일정 시간 동안 응답이 없을 때 알리는 용도
    //해당 세션을 실제로 연결 끊을 건지 아닌지는 사용자가 판단
    //무시하면 카운터가 초기화되고, 다시 타임아웃 시간 측정
    virtual void OnSessionTimeout(SessionID sessionId, timeoutInfo info)
    {
    }

    virtual void OnMonitorRun()
    {
    }

    //MONITOR
    psh::uint64 GetAcceptCount() const;
    psh::uint64 GetAcceptTps() const;
    psh::uint64 GetRecvTps() const;
    psh::uint64 GetSendTps() const;
    psh::int16 GetSessionCount() const;
    psh::uint64 GetPacketPoolSize() const;
    psh::uint32 GetPacketPoolEmptyCount() const;
    psh::uint64 GetTimeoutCount() const;
    static psh::uint32 GetPacketPoolAcquireCount();
    static psh::uint32 GetPacketPoolReleaseCount();
    psh::uint64 GetDisconnectCount() const;
    psh::uint64 GetDisconnectPerSec() const;
    psh::uint32 GetSemaphoreTimeoutCount() const;

    size_t GetPageFaultCount() const;
    size_t GetPeakPMemSize() const;
    size_t GetPMemSize() const;
    size_t GetPeakVirtualMemSize() const;
    size_t GetVirtualMemSize() const;
    size_t GetPeakPagedPoolUsage() const;
    size_t GetPeakNonPagedPoolUsage() const;
    size_t GetPagedPoolUsage() const;
    size_t GetNonPagedPoolUsage() const;

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

    void SpawnThread(std::function<void()> fn);
    void ReleaseAliveRef();

    //WorkerThreadFunc
    void GroupThread();
    void WorkerThread();
    void AcceptThread();
    void MonitorThread();
    void TimeoutThread();
};


template <typename GroupType, typename... Args>
GroupID IOCP::CreateGroup(Args&&... args) const
{
    return _groupManager->CreateGroup<GroupType>(std::forward<Args>(args)...);
}
