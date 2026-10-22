#pragma once
#include "GroupTypes.h"
#include "Executable.h"
#include "LockFreeFixedQueue.h"
#include "Logger.h"
#include "Metrics.h"
#include "SessionTypes.h"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <memory>

class Session;
class SendBuffer;
class GroupManager;
class RecvBuffer;
class IOCP;

/// <summary>
/// 구현상의 한계로 인해 생성자에서 할 동작들 중 그룹 관련된 기능(groupID 가지고
/// 오는 등)은 OnCreate에서 설정해야 함.
/// </summary>
class Group
{

    friend class GroupManager;
    friend class Session;
public:
    Group();
    virtual ~Group();
    Group(const Group& other) = delete;
    Group(Group&& other) noexcept = delete;
    Group& operator=(const Group& other) = delete;
    Group& operator=(Group&& other) noexcept = delete;

    /// <summary>
    /// OnCreate를 리턴한 이후부터 해당 그룹으로 이동 가능
    /// </summary>
    virtual void OnCreate();
    virtual void OnUpdate(int milli);
    virtual void OnEnter(SessionID id);
    virtual void OnLeave(SessionID id, int wsaErrCode);
    virtual void OnRecv(SessionID id, RecvBuffer& recvBuffer);

    [[nodiscard]] GroupID GetGroupID() const;
    void SendPacket(SessionID id, const SendBuffer& buffer) const;
    // void SendPackets(SessionID id, Vector<SendBuffer>& buffer);

    // static constexpr int debugSize = 2001;

    // enum jobType {
    //	Enter,
    //	Leave,
    //	Packet,
    //	Other
    // };
    // struct stDebugData
    //{
    //	SessionID id;
    //	jobType type;
    //	GroupID dst = InvalidGroupID();
    //	String cause;
    // };

    // struct stDebug
    //{
    //	long arrIndex;
    //	stDebugData arr[1000];
    // };
    ////세션 id, 종류
    // stDebug debugArr[debugSize];
    ////tuple<SessionID, String, thread::id> _debugLeave[debugSize];

    // void Write(SessionID id, jobType type,GroupID dst, String cause)
    //{

    //	auto index = ++debugArr[id.index].arrIndex;

    //	if (type == Leave && debugArr[id.index].arr[index - 1].id != id)
    //		psh::util::DebugBreak();

    //	debugArr[id.index].arr[index].id = id;
    //	debugArr[id.index].arr[index].type = type;
    //	debugArr[id.index].arr[index].dst = dst;
    //	debugArr[id.index].arr[index].cause = cause;

    //}
protected:
    void MoveSession(SessionID id, GroupID dst) const;
    void LeaveSession(SessionID id, int wsaErrCode);
    void EnterSession(SessionID id);
    void SetLoopMs(int loopMS);

    [[nodiscard]] size_t Sessions() const;

    IOCP* _iocp;
    int _loopMs = 10;
private:
    struct GroupJob
    {
        enum type { Recv, Enter, Move, Leave };

        type type{};
        SessionID sessionId{};
        int errCode = -1;
        GroupID dst = GroupID(0);
        std::size_t transferred{0};
    };
    enum class UpdateState
    {
        Idle,
        Scheduled,
        Running,
        //  실행 중에 업데이트 요청이 들어와 한 번 더 돌아야 하는 상태
        RunningDirty
    };

    void Enqueue(GroupJob job);

    //  그룹 ID를 속성으로 쓰므로 ID가 정해진 뒤 GroupManager가 호출한다.
    void InitMetrics();

    bool NeedUpdate();
    //  Idle이면 업데이트를 post하고, 실행 중이면 한 번 더 돌도록 표시한다.
    void RequestUpdate();
    void Update();
    void Process();
    [[nodiscard]] bool hasJob() const;

    bool HandleJob();

    void onRecvPacket(const Session& session, RecvBuffer& buffer);
    std::unique_ptr<Logger> _groupLogger;
    GroupID _groupId = GroupID::InvalidGroupID();
    std::unique_ptr<Executable> _executable;
    std::chrono::steady_clock::time_point _prevUpdate;
    std::chrono::steady_clock::time_point _nextUpdate;

    SessionSet _sessions;
    std::unique_ptr<LockFreeFixedQueue<GroupJob, 8192>> _jobs;

    // TlsLockFreeQueue<GroupJob> _jobs;

    std::atomic<UpdateState> _updateState = UpdateState::Idle;

    GroupManager* _owner;

    long debugIndex = 0;

    // MONITOR
    std::chrono::steady_clock::time_point _nextMonitor;
    psh::metrics::Histogram _processMetric;
    psh::metrics::Counter _jobMetric;
    psh::metrics::Counter _enterMetric;
    psh::metrics::Counter _leaveMetric;
    //  다른 스레드에서 읽으면 안 되는 값이라 콜백 대신 모니터 주기마다 Set한다.
    psh::metrics::Gauge _sessionMetric;
    psh::metrics::Gauge _jobQueueMetric;
};
