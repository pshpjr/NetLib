#include "Group.h"
#include "Utility.h"

#include <memory>

#include "Logger.h"
#include "RecvBuffer.h"
#include "Executable.h"
#include "GroupManager.h"
#include "IOCP.h"
#include "Profiler.h"
#include "Protocol.h"
#include "Profiler.h"
#include "stdafx.h"

Group::Group()
	: _iocp(nullptr), _groupLogger(std::make_unique<Logger>(
		  L"GroupLogger", Logger::LogLevel::Debug)),
	  _executable(std::make_unique<Executable>(Executable::IoType::Group,
		  [this](NetError, std::size_t) { Update(); })),
	  _prevUpdate(std::chrono::steady_clock::now()),
	  _nextUpdate(_prevUpdate + std::chrono::milliseconds(_loopMs)),
	  _jobs(std::make_unique<LockFreeFixedQueue<GroupJob, 8192>>()),
	  _owner(nullptr), _nextMonitor(_prevUpdate + std::chrono::seconds(1))
{
}

Group::~Group() = default;

void Group::InitMetrics()
{
	using psh::metrics::Registry;
	const psh::metrics::Attributes attributes{{"group", std::to_string(static_cast<long>(_groupId))}};

	_processMetric = Registry::CreateTimeHistogram("group.process.duration", "time spent in one Process call", attributes);
	_jobMetric = Registry::CreateCounter("group.jobs", "handled group jobs", attributes);
	_enterMetric = Registry::CreateCounter("group.enters", "sessions entered the group", attributes);
	_leaveMetric = Registry::CreateCounter("group.leaves", "sessions left the group", attributes);
	_sessionMetric = Registry::CreateGauge("group.sessions", "sessions in the group", attributes);
	_jobQueueMetric = Registry::CreateGauge("group.job.queue.size", "queued group jobs", attributes);
}

void Group::Enqueue(GroupJob job)
{
	{
		// 그룹이 가득 찰 정도면 서버가 엄청 바쁜 거
		PRO_FUNCTION();
		while (!_jobs->Enqueue(job))
		{
			std::this_thread::yield();
		}
	}
	RequestUpdate();
}

bool Group::NeedUpdate()
{
	return std::chrono::steady_clock::now() >= _nextUpdate;
}

void Group::RequestUpdate()
{
	auto state = _updateState.load();
	while (true)
	{
		if (state == UpdateState::Scheduled || state == UpdateState::RunningDirty)
		{
			return;
		}
		
		//  idle이면 scheduled로 바꾸고, running이면 runningDirty로 바꾼다
		const auto next = state == UpdateState::Idle ? UpdateState::Scheduled : UpdateState::RunningDirty;
		if (_updateState.compare_exchange_weak(state, next))
		{
			//  내가 scheduled로 바꿨을때만 post 한다. 
			if (next == UpdateState::Scheduled)
			{
				_iocp->PostExecutable(*_executable);
			}
			return;
		}
	}
}

void Group::Update()
{
    PRO_BEGIN("Group::Update");
    PRO_VALUE(static_cast<long>(_groupId));
	auto expected = UpdateState::Scheduled;
	if (!_updateState.compare_exchange_strong(expected, UpdateState::Running))
	{
		psh::util::DebugBreak();
	}

	while (true)
	{
		Process();

		//  계속 running 상태면 아무도 추가하지 않았다. 
		expected = UpdateState::Running;
		if (_updateState.compare_exchange_strong(expected, UpdateState::Idle))
		{
			return;
		}

		//  dirty 상태면 running으로 돌리고 한번 더 돈다. 
		_updateState.store(UpdateState::Running);
	}
}

void Group::Process()
{
	using namespace std::chrono;
	using namespace std::literals::chrono_literals;
	PRO_BEGIN("Group::Process");
    PRO_VALUE(static_cast<long>(_groupId));
    PRO_PLOT("Group queued jobs", static_cast<double>(_jobs->Size()));

	const auto metricStart = psh::metrics::Histogram::TimePoint::Now();
	while (hasJob() || NeedUpdate())
	{
		HandleJob();

		if (NeedUpdate())
		{
			{
                PRO_BEGIN("Group::OnUpdate");
                PRO_VALUE(static_cast<long>(_groupId));
                OnUpdate(_loopMs);
            }
			_nextUpdate += milliseconds(_loopMs);
		}
	}

	_processMetric.RecordSince(metricStart);

	if (steady_clock::now() > _nextMonitor)
	{
		_sessionMetric.Set(static_cast<psh::int64>(_sessions.size()));
		_jobQueueMetric.Set(_jobs->Size());

		_nextMonitor += 1s;
	}
}

bool Group::HandleJob()
{
	GroupJob job;
	int jobs = 0;
	while (_jobs->Dequeue(job))
	{
		PRO_BEGIN("HandleJob");
        PRO_VALUE((static_cast<unsigned long long>(static_cast<long>(_groupId)) << 32) | static_cast<unsigned>(job.type));
		++jobs;
		switch (job.type)
		{
		case GroupJob::type::Enter:
			{
				_enterMetric.Add();

				// Write(job.sessionId, Group::jobType::Enter, InvalidGroupID(), L"Enter
				// Handled");
				_sessions.insert(job.sessionId);

				OnEnter(job.sessionId);

                //  세션 정리 중일 경우 x
			    if (auto session = _iocp->FindSession(job.sessionId, L"EnterComplete"))
                {
                    session->CompleteMove();
                    session->Release(L"EnterComplete");
                }
			}
			break;
		case GroupJob::type::Leave:
			{
				_leaveMetric.Add();
				// Write(job.sessionId, Group::jobType::Leave, InvalidGroupID(), L"Leave
				// Handled");
				if (_sessions.erase(job.sessionId) == 0)
				{
					psh::util::DebugBreak();
				}
				OnLeave(job.sessionId, job.errCode);

				_iocp->CheckPostRelease(job.sessionId, _groupId);
			}
			break;
		// 처음에 만들 때 있었다가 구조 변경되면서 안 쓰는 것 같음
		// TODO: 확인 후 지우기
		case GroupJob::type::Move:
			{
				// Write(job.sessionId, Group::jobType::Leave, InvalidGroupID(), L"Leave
				// Handled");
				if (_sessions.erase(job.sessionId) == 0)
				{
					//
					psh::util::DebugBreak();
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
					// Write(job.sessionId, Group::jobType::Other, InvalidGroupID(),
					// L"InvalidSession when recv");
					continue;
				}

				session->ProcessRecv(this);
				session->Release(L"HandlePacketRelease");
			}
			break;
		}


		// 주기적으로 잡 탈출 해서 게임 루프 업데이트 해야 하는지 확인
		if (jobs == 1000)
		{
			break;
		}
	}
	_jobMetric.Add(jobs);
	return true;
}

void Group::onRecvPacket(const Session& session, RecvBuffer& buffer)
{
	try
	{
		OnRecv(session._sessionId, buffer);
		ASSERT_CRASH(buffer.CanPopSize() == 0, "can pop from recv buffer");
	}
	catch (const std::invalid_argument&)
	{
		_iocp->DisconnectSession(session.GetSessionId());
		_groupLogger->Write(L"Recv Pop Err", Logger::LogLevel::Debug, L"ERR");
	}
	catch (const std::exception& e)
	{
		_iocp->DisconnectSession(session.GetSessionId());
		_groupLogger->Write(L"Recv Exception", Logger::LogLevel::Error, L"sessionID : %d, group : %d, error : %s",
		                    session.GetSessionId(), _groupId, e.what());
	}
}

// 해당 세션이 언제 나갈지 모르는데 상관 없나?
// 어짜피 동시에 처리되는 건 아니니까 상관 x.
void Group::LeaveSession(const SessionID id, int wsaErrCode)
{
	Enqueue({GroupJob::type::Leave, id, wsaErrCode});
}

void Group::EnterSession(const SessionID id)
{
	Enqueue({GroupJob::type::Enter, id});
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
}

void Group::OnEnter(SessionID id)
{
}

void Group::OnLeave(SessionID id, int wsaErrCode)
{
}

void Group::OnRecv(SessionID id, RecvBuffer& recvBuffer)
{
}

GroupID Group::GetGroupID() const
{
	return _groupId;
}

void Group::SendPacket(const SessionID id, const SendBuffer& buffer) const
{
	_iocp->SendPacket(id, buffer);
	// 그룹의 경우에는 직접 send 때리는 것이 더 좋을 수 있다. 그룹은 iocp
	// 스레드에서 돌고 있음.
	//  _iocp->SendPacketDirect(id, buffer);
}

// void Group::SendPackets(SessionID id, Vector<SendBuffer>& buffer)
// {
// 	_iocp->SendPackets(id, buffer);
// }

void Group::MoveSession(const SessionID id, const GroupID dst) const
{
	_owner->MoveSession(id, dst);
}

bool Group::hasJob() const
{
	return _jobs->Size() > 0;
}
