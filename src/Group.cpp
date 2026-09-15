#include "Group.h"

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
		  [this](ULONG_PTR, DWORD, void*) { Update(); })),
	  _prevUpdate(std::chrono::steady_clock::now()),
	  _nextUpdate(_prevUpdate + std::chrono::milliseconds(_loopMs)),
	  _jobs(std::make_unique<LockFreeFixedQueue<GroupJob, 8192>>()),
	  _owner(nullptr), _nextMonitor(_prevUpdate + std::chrono::seconds(1))
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
}

psh::int64 Group::GetMaxWorkTime() const
{
	return oldMaxWorkTime;
}

psh::int32 Group::GetEnterTps() const
{
	return _enterTps;
}

psh::int32 Group::GetLeaveTps() const
{
	return _leaveTps;
};

bool Group::Enqueue(GroupJob job, bool update)
{
	{
		// 그룹이 가득 찰 정도면 서버가 엄청 바쁜 거
		PRO_FUNCTION();
		while (!_jobs->Enqueue(job))
		{
			std::this_thread::yield();
		}
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
	using namespace std::chrono;
	using namespace std::literals::chrono_literals;
	if (_isRun.exchange(true))
	{
		return;
	}
	PRO_FRAME("Group HandleJob");

	auto start = steady_clock::now();
	while (hasJob() || NeedUpdate())
	{
		HandleJob();

		if (NeedUpdate())
		{
			OnUpdate(_loopMs);
			_nextUpdate += milliseconds(_loopMs);
		}
	}

	auto end = steady_clock::now();
	auto curWorkTime = end - start;
	maxWorkTime = std::max(maxWorkTime, curWorkTime);
	workTime += curWorkTime;

	if (end > _nextMonitor)
	{
		oldMaxWorkTime =
			std::chrono::duration_cast<milliseconds>(maxWorkTime).count();
		maxWorkTime = maxWorkTime.zero();
		oldWorkTime = std::chrono::duration_cast<milliseconds>(workTime).count();
		workTime = workTime.zero();

		oldHandledJob = _handledJob.exchange(0);
		_enterTps = std::exchange(_handledEnter, 0);
		_leaveTps = std::exchange(_handledLeave, 0);
		jobQSize = _jobs->Size();

		_nextMonitor += 1s;
	}
	_isRun.store(false);
}

bool Group::HandleJob()
{
	GroupJob job;
	int jobs = 0;
	while (_jobs->Dequeue(job))
	{
		PRO_BEGIN("HandleJob");
		++jobs;
		switch (job.type)
		{
		case GroupJob::type::Enter:
			{
				++_handledEnter;

				// Write(job.sessionId, Group::jobType::Enter, InvalidGroupID(), L"Enter
				// Handled");
				_sessions.insert(job.sessionId);

				OnEnter(job.sessionId);
			}
			break;
		case GroupJob::type::Leave:
			{
				++_handledLeave;
				// Write(job.sessionId, Group::jobType::Leave, InvalidGroupID(), L"Leave
				// Handled");
				if (_sessions.erase(job.sessionId) == 0)
				{
					__debugbreak();
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
					// Write(job.sessionId, Group::jobType::Other, InvalidGroupID(),
					// L"InvalidSession when recv");
					continue;
				}

				/*
				 * 지금 받은 패킷만 처리하고 있음. 처음에는 해당 세션이 받은 모든 패킷 다 처리하게 만들었는데
				 * 이 경우 패킷이 계속 들어오면 뒤애 애들 처리가 너무 느려지는 문제 발생
				 * 전체 처리량이 줄어들더라도 반응성을 위해 수정함.
				 */
				if (const char sKey = session->_staticKey)
				{
					RecvHandler<NetHeader>(*session, _iocp, job.transferred);
				}
				else
				{
					RecvHandler<LANHeader>(*session, _iocp, job.transferred);
				}

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
	_handledJob.fetch_add(jobs);
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
}

// 해당 세션이 언제 나갈지 모르는데 상관 없나?
// 어짜피 동시에 처리되는 건 아니니까 상관 x.
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

void Group::OnRecv(SessionID id, RecvBuffer& recvBuffer)
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

void Group::SendPacket(const SessionID id, const SendBuffer& buffer) const
{
	_iocp->SendPacket(id, buffer);
	// 그룹의 경우에는 직접 send 때리는 것이 더 좋을 수 있다. 그룹은 iocp
	// 스레드에서 돌고 있음.
	//  _iocp->SendPacketBlocking(id, buffer);
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
void Group::RecvHandler(Session& session, void* iocp, DWORD transferred)
{
	IOCP& server = *static_cast<IOCP*>(iocp);
	int loopCount = 0;
	DWORD handleBytes = 0;

	RingBuffer& recvQ = session._recvQ;
	session.setRecvHandled(false);
	while (true)
	{
		if (session.GetGroupID() != _groupId)
		{
			// Write(session.GetSessionId(), Other, session.GetGroupID(), L"Not this
			// Group");
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
				_groupLogger->Write(
					L"Disconnect", Logger::LogLevel::Debug,
					L"Group WrongPacketCode, SessionID : %d\n frontIndex: %d \n", id,
					frontIndex);
				session.Close();
				break;
			}

			if (header.len > session._maxPacketLen)
			{
				int frontIndex = recvQ.GetFrontIndex();
				SessionID id = session.GetSessionId();
				_groupLogger->Write(
					L"Disconnect", Logger::LogLevel::Debug,
					L"Group WrongPacketLen, SessionID : %d\n index: %d \n", id,
					frontIndex);
				session.Close();
				break;
			}
		}
		const int totPacketSize = header.len + sizeof(Header);
		if (recvQ.Size() < totPacketSize)
		{
			break;
		}

		RecvBuffer buffer(&recvQ, header.len);
		if constexpr (std::is_same_v<Header, NetHeader>)
		{
			recvQ.Decode(session._staticKey);

			if (!recvQ.ChecksumValid())
			{
				_groupLogger->Write(L"Disconnect", Logger::LogLevel::Debug,
				                    L"Group Invalid Checksum %d",
				                    session.GetSessionId());
				session.Close();
				break;
			}
		}
		loopCount++;
		recvQ.Dequeue(sizeof(Header));
		onRecvPacket(session, buffer);
		handleBytes += totPacketSize;
	}

	if (loopCount > 0)
	{
		server.IncreaseRecvCount(loopCount);
	}
}
