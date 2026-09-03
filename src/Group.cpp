#include "Group.h"

#include <memory>

#include "CLogger.h"
#include "CRecvBuffer.h"
#include "GroupExecutable.h"
#include "GroupManager.h"
#include "IOCP.h"
#include "Profiler.h"
#include "Protocol.h"
#include "stdafx.h"


Group::Group()
    : _iocp(nullptr)
    , _executable(std::make_unique<GroupExecutable>(this))
    , _prevUpdate(std::chrono::steady_clock::now())
    , _nextUpdate(_prevUpdate + std::chrono::milliseconds(_loopMs))
    , _owner(nullptr)
    , _nextMonitor(_prevUpdate + std::chrono::seconds(1))
    ,_groupLogger(std::make_unique<CLogger>(L"GroupLogger",CLogger::LogLevel::Debug))
,_jobs(std::make_unique<LockFreeFixedQueue<GroupJob,8192>>())
{
}

Group::~Group() = default;

psh::int64 Group::GetWorkTime() const
{
    return oldWorkTime;
}

psh::int64 Group::GetJobTps() const
{
    return oldHandledJob;
};

bool Group::Enqueue(GroupJob job, bool update)
{
    while (!_jobs->Enqueue(job))
    {
    }

    if (update && _isRun == 0)
    {
        Update();
        return true;
    }
    return false;
}

bool Group::NeedUpdate()
{
    return std::chrono::steady_clock::now() >= _nextUpdate;
}

void Group::Update()
{
    using std::chrono::duration_cast;
    using std::chrono::steady_clock;
    using std::chrono::milliseconds;
    using std::literals::chrono_literals::operator""s;
    if (InterlockedExchange8(&_isRun, 1) == 1)
    {
        return;
    }


    auto start = steady_clock::now();
    while (hasJob() || NeedUpdate())
    {
        HandleJob();

        if (NeedUpdate())
        {
            //auto now = steady_clock::now();
            //auto duration = duration_cast<milliseconds>( now - _prevUpdate);
            //OnUpdate(duration.count());
            //_prevUpdate += duration;

            //while (now > _nextUpdate)
            //{
            //    _nextUpdate += milliseconds(_loopMs);
            //}

            OnUpdate(_loopMs);
            _nextUpdate += milliseconds(_loopMs);
        }
    }

    auto end = steady_clock::now();

    workTime += duration_cast<milliseconds>(end - start).count();


    if (end > _nextMonitor)
    {
        oldWorkTime = workTime;
        workTime = 0;
        jobQSize = _jobs->Size();

        oldHandledJob = _handledJob;
        _handledJob = 0;

        _enterTps = _handledEnter;
        _handledEnter = 0;
        _leaveTps = _handledLeave;
        _handledLeave = 0;

        _nextMonitor += 1s;
    }
    InterlockedExchange8(&_isRun, 0);
}


bool Group::HandleJob()
{
    GroupJob job;

    int jobs = 0;
    while (_jobs->Dequeue(job))
    {
        ++jobs;
        switch (job.type)
        {
        case GroupJob::type::Enter:
            {
            ++_handledEnter;

            //Write(job.sessionId, Group::jobType::Enter, InvalidGroupID(), L"Enter Handled");
            _sessions.insert(job.sessionId);

            OnEnter(job.sessionId);
            }
            break;
        case GroupJob::type::Leave:
            {
            ++_handledLeave;
            //Write(job.sessionId, Group::jobType::Leave, InvalidGroupID(), L"Leave Handled");
            if (_sessions.erase(job.sessionId) == 0)
            {
                __debugbreak();
            }
            OnLeave(job.sessionId, job.errCode);

            _iocp->CheckPostRelease(job.sessionId, _groupId);
            }
            break;

        case GroupJob::type::Move:
            {
            //Write(job.sessionId, Group::jobType::Leave, InvalidGroupID(), L"Leave Handled");
            if (_sessions.erase(job.sessionId) == 0)
            {
                __debugbreak();
            }
            OnLeave(job.sessionId, job.errCode);

            _iocp->CheckPostRelease(job.sessionId, _groupId);
            }
            break;
        case GroupJob::type::Recv:
            {
            Session* session;

            session = _iocp->FindSession(job.sessionId, L"HandlePacket");

            if (session == nullptr)
            {
                //Write(job.sessionId, Group::jobType::Other, InvalidGroupID(), L"InvalidSession when recv");
                continue;
            }

            if (const char sKey = session->_staticKey)
            {
                RecvHandler<NetHeader>(*session, _iocp);
            }
            else
            {
                RecvHandler<LANHeader>(*session, _iocp);
            }
            session->RecvNotIncrease();
            session->Release(L"HandlePacketRelease");
            }
            break;
        }
    }
    InterlockedAdd64(&_handledJob, jobs);
    return true;
}

void Group::onRecvPacket(const Session& session, CRecvBuffer& buffer)
{
    try
    {
        OnRecv(session._sessionId, buffer);
        ASSERT_CRASH(buffer.CanPopSize() == 0, "can pop from recv buffer");
    }
    catch (const std::invalid_argument&)
    {
        _iocp->DisconnectSession(session.GetSessionId());
        _groupLogger->Write(L"Recv Pop Err", CLogger::LogLevel::Debug, L"ERR");
    }
}


//해당 세션이 언제 나갈지 모르는데 상관 없나?
//어짜피 동시에 처리되는 건 아니니까 상관 x.
void Group::LeaveSession(const SessionID id, bool update, int wsaErrCode)
{
    Enqueue({GroupJob::type::Leave, id, wsaErrCode}, update);
}

void Group::EnterSession(const SessionID id, bool update)
{
    Enqueue({GroupJob::type::Enter, id}, update);
}

void Group::SetLoopMs(int loopMS)
{
    _loopMs = loopMS;
}

size_t Group::Sessions() const
{
    return _sessions.size();
}

void Group::OnCreate()
{
}

void Group::OnUpdate(int milli)
{
    UNREFERENCED_PARAMETER(milli);
}

void Group::OnEnter(SessionID id)
{
    UNREFERENCED_PARAMETER(id);
}

void Group::OnLeave(SessionID id, int wsaErrCode)
{
    UNREFERENCED_PARAMETER(id);
}

void Group::OnRecv(SessionID id, CRecvBuffer &recvBuffer)
{
    UNREFERENCED_PARAMETER(id);
    UNREFERENCED_PARAMETER(recvBuffer);
}



GroupID Group::GetGroupID() const
{
    return _groupId;
}

int Group::GetQueued() const
{
    return jobQSize;
}

void Group::SendPacket(const SessionID id, SendBuffer& buffer) const
{

    //그룹의 경우에는 직접 send 때리는 것이 더 좋을 수 있다. 그룹은 iocp 스레드에서 돌고 있음.
    // _iocp->SendPacketBlocking(id, buffer);
}

void Group::SendPackets(SessionID id, Vector<SendBuffer>& buffer)
{
    _iocp->SendPackets(id, buffer);
}

void Group::MoveSession(const SessionID id, const GroupID dst) const
{
    _owner->MoveSession(id, dst, false);
}

void Group::Execute(IOCP* iocp) const
{
    _executable->Execute(0, static_cast<int>(Executable::IoType::Group), iocp);
}

bool Group::hasJob() const
{
    return _jobs->Size() > 0;
}


template <typename Header>
void Group::RecvHandler(Session& session, void* iocp)
{
    IOCP& server = *static_cast<IOCP*>(iocp);
    int loopCount = 0;

    CRingBuffer& recvQ = session._recvQ;
    while (true)
    {
        if (session.GetGroupID() != _groupId)
        {
            //Write(session.GetSessionId(), Other, session.GetGroupID(), L"Not this Group");
            break;
        }

        if (recvQ.Size() < sizeof(Header))
        {
            break;
        }


        Header header;
        recvQ.Peek(reinterpret_cast<char*>(&header), sizeof(Header));

        if constexpr (std::is_same_v<Header, NetHeader>)
        {
            if (header.code != dfPACKET_CODE)
            {
                int frontIndex = recvQ.GetFrontIndex();
                SessionID id = session.GetSessionId();
                _groupLogger->Write(L"Disconnect", CLogger::LogLevel::Debug
                    , L"Group WrongPacketCode, SessionID : %d\n frontIndex: %d \n", id, frontIndex);
                session.Close();
                break;
            }

            if (header.len > session._maxPacketLen)
            {
                int frontIndex = recvQ.GetFrontIndex();
                SessionID id = session.GetSessionId();
                _groupLogger->Write(L"Disconnect", CLogger::LogLevel::Debug
                    , L"Group WrongPacketLen, SessionID : %d\n index: %d \n", id, frontIndex);
                session.Close();
                break;
            }
        }

        if (const int totPacketSize = header.len + sizeof(Header);
            recvQ.Size() < totPacketSize)
        {
            break;
        }

        CRecvBuffer buffer(&recvQ, header.len);
        if constexpr (std::is_same_v<Header, NetHeader>)
        {
            recvQ.Decode(session._staticKey);

            if (!recvQ.ChecksumValid())
            {
                _groupLogger->Write(L"Disconnect", CLogger::LogLevel::Debug, L"Group Invalid Checksum %d"
                    , session.GetSessionId());
                session.Close();
                break;
            }
        }
        loopCount++;
        recvQ.Dequeue(sizeof(Header));
        onRecvPacket(session, buffer);
    }

    if (loopCount > 0)
    {
        server.IncreaseRecvCount(loopCount);
    }
}
