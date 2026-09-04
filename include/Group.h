#pragma once

#include <atomic>
#include <chrono>


#include "GroupTypes.h"

#include "SessionTypes.h"
#include "LockFreeFixedQueue.h"


class Logger;
class Session;
class SendBuffer;
class GroupManager;
class GroupExecutable;
class RecvBuffer;
class IOCP;


/// <summary>
/// 구현상의 한계로 인해 생성자에서 할 동작들 중 그룹 관련된 기능(groupID 가지고 오는 등)은
/// OnCreate에서 설정해야 함. 
/// </summary>
class Group
{
    struct GroupJob
    {
        enum type
        {
            Recv
            , Enter
            , Move
            , Leave
        };

        type type{};
        SessionID sessionId{};
        int errCode = -1;
        GroupID dst = GroupID(0);
    };

    friend class GroupExecutable;
    friend class GroupManager;

public:

    Group();
    virtual ~Group();
    Group(const Group &other) = delete;
    Group(Group &&other) noexcept = delete;
    Group & operator=(const Group &other) = delete;
    Group & operator=(Group &&other) noexcept = delete;

    /// <summary>
    /// OnCreate를 리턴한 이후부터 해당 그룹으로 이동 가능
    /// </summary>
    virtual void OnCreate();
    virtual void OnUpdate(int milli);
    virtual void OnEnter(SessionID id);
    virtual void OnLeave(SessionID id, int wsaErrCode);
    virtual void OnRecv(SessionID id, RecvBuffer& recvBuffer);


    [[nodiscard]] GroupID GetGroupID() const;
    [[nodiscard]] int GetQueued() const;
    void SendPacket(SessionID id, SendBuffer& buffer) const;
    void SendPackets(SessionID id, Vector<SendBuffer>& buffer);

    //static constexpr int debugSize = 2001;


    //enum jobType {
    //	Enter,
    //	Leave,
    //	Packet,
    //	Other
    //};
    //struct stDebugData
    //{
    //	SessionID id;
    //	jobType type;
    //	GroupID dst = InvalidGroupID();
    //	String cause;
    //};

    //struct stDebug
    //{
    //	long arrIndex;
    //	stDebugData arr[1000];
    //};
    ////세션 id, 종류
    //stDebug debugArr[debugSize];
    ////tuple<SessionID, String, thread::id> _debugLeave[debugSize];

    //void Write(SessionID id, jobType type,GroupID dst, String cause)
    //{

    //	auto index = ++debugArr[id.index].arrIndex;

    //	if (type == Leave && debugArr[id.index].arr[index - 1].id != id)
    //		__debugbreak();

    //	debugArr[id.index].arr[index].id = id;
    //	debugArr[id.index].arr[index].type = type;
    //	debugArr[id.index].arr[index].dst = dst;
    //	debugArr[id.index].arr[index].cause = cause;

    //}


protected:
    void MoveSession(SessionID id, GroupID dst) const;
    void LeaveSession(SessionID id, bool update, int wsaErrCode);
    void EnterSession(SessionID id, bool update);
    void SetLoopMs(int loopMS);

    [[nodiscard]] size_t Sessions() const;


    IOCP* _iocp;
    int _loopMs = 10;

    [[nodiscard]] psh::int64 GetWorkTime() const;

    [[nodiscard]] psh::int64 GetJobTps() const;

    [[nodiscard]] psh::int32 GetEnterTps() const
    {
        return _enterTps;
    }

    [[nodiscard]] psh::int32 GetLeaveTps() const
    {
        return _leaveTps;
    }
private:
    bool Enqueue(GroupJob job, bool update = true);

    bool NeedUpdate();
    void Update();
    void Execute(IOCP* iocp) const;
    [[nodiscard]] bool hasJob() const;

    bool HandleJob();

    template <typename Header>
    void RecvHandler(Session& session, void* iocp);


    void onRecvPacket(const Session& session, RecvBuffer& buffer);



private:
    std::unique_ptr<Logger> _groupLogger;
    GroupID _groupId = GroupID::InvalidGroupID();
    std::unique_ptr<GroupExecutable> _executable;
    std::chrono::steady_clock::time_point _prevUpdate;
    std::chrono::steady_clock::time_point _nextUpdate;

    SessionSet _sessions;
    std::unique_ptr<LockFreeFixedQueue<GroupJob,8192>> _jobs;

    //TlsLockFreeQueue<GroupJob> _jobs;

    std::atomic_bool _isRun = false;

    GroupManager* _owner;


    long debugIndex = 0;


    //MONITOR
    std::chrono::steady_clock::time_point _nextMonitor;
    psh::int64 workTime = 0;
    std::atomic<psh::int64> _handledJob = 0;
    long jobQSize = 0;

    psh::int64 oldWorkTime = 0;
    psh::int64 oldHandledJob = 0;
    long _leaveTps = 0;
    long _handledLeave = 0;
    long _enterTps = 0;
    long _handledEnter = 0;


};
