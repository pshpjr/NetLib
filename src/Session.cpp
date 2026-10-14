#include "Profiler.h"
#include "Session.h"
#include "Buffer.h"

#include <iostream>

#include <optional>

#include "SendBufferImpl.h"
#include "IOCP.h"

#include "Logger.h"
#include "CoreGlobal.h"
#include "RecvBuffer.h"
#include "Executable.h"
#include "Protocol.h"

// Construction
Session::Session()
	: _sessionId()
	  , _socket({})
	  , _sendingQ()
	  , _owner(nullptr)
	  , _releaseExecutable(*new Executable(Executable::IoType::Release))
{
	_releaseExecutable.SetCallback([this](NetError, std::size_t)
	{
		_owner->_onDisconnect(GetSessionId(), GetErr());
		Reset();
		const unsigned short index = GetSessionId().index;
		_owner->ReleaseFreeIndex(index);
	});
}

Session::Session(Socket socket, SessionID sessionId, IOCP& owner)
    : Session()
{
    _sessionId = sessionId;
	SetSocket(std::move(socket));
    _owner = &owner;
}

// Identity
SessionID Session::GetSessionId() const
{
	return _sessionId;
}

void Session::SetSessionId(const SessionID sessionID)
{
	_sessionId = sessionID;
}

// Connection
Ip Session::GetIp() const
{
	return _socket.GetIp();
}

Port Session::GetPort() const
{
	return _socket.GetPort();
}

SockAddr Session::GetAddress() const
{
	return _socket.GetSockAddress();
}

void Session::SetSocket(Socket&& socket)
{
	_socket = std::move(socket);

	_socket.SetCallbacks(
    [this](NetError error, std::size_t transferred)
    {
        if (error == NetErrc::None && transferred == 0)
        {
            Release(L"ConnectEndRel", 0);
            return;
        }

        if (error != NetErrc::None)
        {
            using namespace std::chrono;

            const int errNo = error.NativeCode;
            const auto* type = L"Recv";
            SetErr(error);

            switch (error)
            {
            case NetErrc::ConnectionReset:
                break;

            case NetErrc::ConnectionAborted:
                gLogger->Write(L"ConnectionError", Logger::LogLevel::Error,
                               L"Connection Aborted sessionID : %d, executableType : %s, errNo : %d",
                               GetSessionId(), type, errNo);
                break;

            case NetErrc::TimedOut:
            {
                const auto now = steady_clock::now();
                const auto recvWait = duration_cast<milliseconds>(now - lastRecv);
                const auto sendWait = _needCheckSendTimeout ? duration_cast<milliseconds>(now - _lastSend) : 0ms;

                gLogger->Write(L"ConnectionError", Logger::LogLevel::Error,
                               L"SemaphoreTimeout  sessionID : %d, executableType : %s, "
                               L"sendWait : %d recvWait : %d, errNo : %d",
                               GetSessionId(), type, sendWait, recvWait, errNo);
                ++_owner->_tcpSemaphoreTimeout;
                break;
            }

            case NetErrc::OperationCanceled:
                gLogger->Write(L"ConnectionError", Logger::LogLevel::Debug,
                               L"Operation Aborted sessionID : %d, executableType : %s, errNo : %d", GetSessionId(),
                               type, errNo);
                break;

            case NetErrc::NotFound:
                gLogger->Write(L"ConnectionError", Logger::LogLevel::Error,
                               L"Error Not Found sessionID : %d, executableType : %s, errNo : %d", GetSessionId(),
                               type, errNo);
                break;

            default:
                psh::util::DebugBreak();
                break;
            }

            Release(L"GQCSErrorRelease", errNo);
            return;
        }
        _recvQ.MoveRear(transferred);

		

		if (auto handoffGroupId = psh::RecvState::MarkRecvDirty(_state); 
		    !handoffGroupId)
		{
            //  없으면 버퍼 소비 권한이 없다. 
		}
        else
        {
            HandOffRecvTo(*handoffGroupId);
        }


        RecvNotIncrease();
    }
	,
    [this](NetError error, std::size_t transferred)
    {
        if (error == NetErrc::None && transferred == 0)
        {
            Release(L"ConnectEndRel", 0);
            return;
        }

        if (error != NetErrc::None)
        {
            using namespace std::chrono;

            const int errNo = error.NativeCode;
            const auto* type = L"Send";
            SetErr(error);

            switch (error)
            {
            case NetErrc::PartialSend:
            {
                std::size_t requestedBytes = 0;
                const auto packetCount = _dataNotSend.load();
                for (int i = 0; i < packetCount; ++i)
                    requestedBytes += _sendingQ[i]->SendDataSize();

                const bool closeRequested = !_connect.load();
                Close();
                gLogger->Write(L"ConnectionError", Logger::LogLevel::Error,
                               L"Partial send sessionID : %llu, requestedBytes : %zu, "
                               L"transferredBytes : %zu, closeRequested : %d, "
                               L"cause : unavailable, action : close",
                               static_cast<unsigned long long>(GetSessionId().id),
                               requestedBytes, transferred, static_cast<int>(closeRequested));
                break;
            }

            case NetErrc::ConnectionReset:
                break;

            case NetErrc::ConnectionAborted:
                gLogger->Write(L"ConnectionError", Logger::LogLevel::Error,
                               L"Connection Aborted sessionID : %d, executableType : %s, errNo : %d",
                               GetSessionId(), type, errNo);
                break;

            case NetErrc::TimedOut:
            {
                const auto now = steady_clock::now();
                const auto recvWait = duration_cast<milliseconds>(now - lastRecv);
                const auto sendWait = _needCheckSendTimeout ? duration_cast<milliseconds>(now - _lastSend) : 0ms;

                gLogger->Write(L"ConnectionError", Logger::LogLevel::Error,
                               L"SemaphoreTimeout  sessionID : %d, executableType : %s, "
                               L"sendWait : %d recvWait : %d, errNo : %d",
                               GetSessionId(), type, sendWait, recvWait, errNo);
                ++_owner->_tcpSemaphoreTimeout;
                break;
            }

            case NetErrc::OperationCanceled:
                gLogger->Write(L"ConnectionError", Logger::LogLevel::Debug,
                               L"Operation Aborted sessionID : %d, executableType : %s, errNo : %d", GetSessionId(),
                               type, errNo);
                break;

            case NetErrc::NotFound:
                gLogger->Write(L"ConnectionError", Logger::LogLevel::Error,
                               L"Error Not Found sessionID : %d, executableType : %s, errNo : %d", GetSessionId(),
                               type, errNo);
                break;

            default:
                psh::util::DebugBreak();
                break;
            }

            Release(L"GQCSErrorRelease", errNo);
            return;
        }
        auto& server = *_owner;
        const auto sentCount = _dataNotSend.exchange(0);
        server.increaseSendCount(sentCount);
        for (int i = 0; i < sentCount; i++)
        {
            _sendingQ[i]->Release(L"PostSendRelease");
        }
        _needCheckSendTimeout = false;
        _isSending = false;
        TrySend();
        Release(L"PostSendRelease");
    });
}

void Session::ProcessRecv(Group* group)
{
	using enum psh::RecvState::Flag;

    const auto executingGroup = group ? group->GetGroupID() : GroupManager::BaseGroupID();
    // recv를 다시 거는 시점부터 session이 정리될 수 있으므로, 세션을 더 건드리지 않는 맨 마지막에 건다.
    bool resumeRecv = false;

    for (;;)
    {
        auto state = LoadState();
        ASSERT_CRASH(state.Has(RecvScheduled), L"소비 권한은 scheduled된 상태여야 함");

        //  이동중인경우 이동 완료된 그룹에서 처리하므로 Dirty만 남긴다.
        if (state.Has(Moving))
        {
            const auto next = state.With(RecvDirty).Without(RecvScheduled);
            if (!_state.compare_exchange_weak(state.data, next))
                continue;
            break;
        }
        // 다른 그룹에 recv 처리를 요청해야 한다. 내가 가진 실행 권한을 전달한다.
        if (state.GetGroupID() != executingGroup)
        {
            _state.fetch_or(RecvDirty);
            HandOffRecvTo(state.GetGroupID());
            break;
        }
        //  처리할 패킷이 추가로 있다.
        if (state.Has(RecvDirty))
        {
            if (!_state.compare_exchange_weak(state.data, state.Without(RecvDirty)))
                continue;
            if (_staticKey)
                RecvHandler<NetHeader>(group);
            else
                RecvHandler<LANHeader>(group);
            continue;
        }
        //  버퍼가 가득 차서 recv가 멈춰있었다. 버퍼를 비웠으니 끝나고 recv를 다시 건다.
        if (state.Has(RecvPaused))
        {
            if (!_state.compare_exchange_weak(state.data, state.Without(RecvPaused)))
                continue;
            resumeRecv = true;
            continue;
        }
        //  더이상 할 작업 없으면 권한를 반납한다.
        if (_state.compare_exchange_weak(state.data, state.Without(RecvScheduled)))
            break;
    }

    if (resumeRecv)
        RecvNotIncrease();
}

//  해당 그룹에게 이 세션의 버퍼 소비 권한을 전달한다. base의 경우 큐가 없으므로 바로 소비한다. 
void Session::HandOffRecvTo(const GroupID group)
{
	//  TODO: 이 때 Post 하는거랑 그냥 실행하는 거랑 차이 확인. 
    if (group == GroupManager::BaseGroupID())
        ProcessRecv(nullptr);
    else
        _owner->_groupManager->OnRecv(GetSessionId(), group, 0);
}

void Session::SetConnect()
{
	_connect = true;
}

void Session::Close()
{
	_connect.store(false);
	_socket.CancelIO();
}

void Session::SetErr(const int errCode)
{
	SetErr(NetError::FromNativeCode(errCode));
}

void Session::SetErr(const NetError error)
{
	if (_errCode == NetErrc::None)
	{
		_errCode = error;
	}
}

int Session::GetErr()
{
	return _errCode.NativeCode;
}

// Lifetime
void Session::Reset()
{
	_recvQ.Clear();
	_isSending = false;
	_needCheckSendTimeout = false;
	_timeout = _defaultTimeout;
	_errCode = {};

	//RecvBuffer* recvBuffer;
	//while ( _groupRecvQ.Dequeue(recvBuffer) )
	//{
	//	recvBuffer->Release(L"ResetRecvRelease");
	//}

	SendBufferImpl* sendBuffer;
	while (_sendQ.Dequeue(sendBuffer))
	{
		sendBuffer->Release(L"ResetSendRelease");
	}
    _state = psh::RecvState{};
	_recvQ.Clear();

	const int notSend = _dataNotSend.exchange(0);

	for (int i = 0; i < notSend; i++)
	{
		_sendingQ[i]->Release(L"ResetSendingRelease");
		_sendingQ[i] = nullptr;
	}

	_socket.Close();
}

void Session::PostRelease()
{
#ifdef SESSION_DEBUG
	ASSERT_CRASH(DebugRef == 0);
	Write(-1, GroupID::InvalidGroupID(), L"PostRelease");
#endif
	_owner->PostExecutable(_releaseExecutable);
}

bool Session::Release(psh::LPCWSTR content, int type)
{
	int refDecResult = --_refCount;

#ifdef SESSION_DEBUG
	Write(_refCount.load(), GroupID::InvalidGroupID(), content);
#endif

	if (refDecResult == 0)
	{
		long expected = 0;
		if (_refCount.compare_exchange_strong(expected, RELEASE_FLAG))
		{
			//InterlockedIncrement(&_owner->_iocpCompBufferSize);

			//InterlockedOr((long*)&(_groupId), 0) == 0

			if (GetGroupID() == GroupManager::BaseGroupID())
			{
#ifdef SESSION_DEBUG
				Write(_refCount.load(), GroupID::InvalidGroupID(), L"SessionGroup 0 PostRelease");
#endif
				PostRelease();
			}
			else
			{
#ifdef SESSION_DEBUG
				Write(_refCount.load(), GroupID::InvalidGroupID(), L"SessionGroup other");
#endif
				_owner->_groupManager->SessionDisconnected(this);
			}
			return true;
		}
	}
	return false;
}

void Session::OffReleaseFlag()
{
	_refCount.fetch_and(~RELEASE_FLAG);
}

psh::RecvState Session::LoadState() const
{
    return _state.load();
}

// Send
bool Session::EnqueueSendData(SendBufferImpl* buffer)
{
	return _sendQ.Enqueue(buffer);
}

void Session::TrySend()
{
	if (!TryAcquireSendFlag())
	{
		return;
	}

	RealSend();
}

bool Session::TryAcquireSendFlag()
{
	PRO_FUNCTION();
	if (_connect == false)
	{
		return false;
	}
	while (true)
	{
		if (_refCount >= RELEASE_FLAG)
		{
			return false;
		}

		if (_sendQ.Size() == 0)
		{
			return false;
		}

		if (_isSending.exchange(true) == true)
		{
			return false;
		}

		if (_sendQ.Size() == 0)
		{
			_isSending.store(false);

			continue;
		}
		break;
	}
	return true;
}

void Session::RegisterRecv()
{
	++_refCount;

	RecvNotIncrease();
}

//  이 함수 호출 이후론 세션이 정리될 수 있다. 함수 마지막에 호출해야 한다. 
void Session::RecvNotIncrease()
{
	if (_refCount >= RELEASE_FLAG)
	{
		Release(L"tryRecvReleaseIOCanceled");
		return;
	}
	if (_connect == false)
	{
		Release(L"RecvReleaseSessionClosed");
		return;
	}
    size_t bufferCount = 1;
	SocketRecvBuffer recvBuffers[2] = {};


	const auto freeSize = _recvQ.FreeSize();
	if (freeSize == 0)
    {
        if (auto handoffGroupId = psh::RecvState::MarkPauseRecv(_state))
        {
            //  handOff
            HandOffRecvTo(*handoffGroupId);
        }
        return;
    }

	const auto recvSize = std::min<unsigned>(_recvQ.DirectEnqueueSize(), freeSize);
	SetSocketBuffer(recvBuffers[0], _recvQ.GetRear(), recvSize);

	//  [□□□■■■■□□] 형태로 데이터 있는 경우
	if (freeSize > recvSize)
	{
		SetSocketBuffer(recvBuffers[1], _recvQ.GetBuffer(), freeSize - recvSize);

		bufferCount = 2;
	}


	lastRecv = std::chrono::steady_clock::now();


	auto beforeSessionId = _sessionId;
	if (const auto recvResult = _socket.Recv({recvBuffers, bufferCount});
		recvResult.HasError())
	{
		const auto error = recvResult.Error();
		if (error == NetErrc::Pending)
		{
			//이렇게 확인하는 이유 : 컨텐츠에서 세션 연결 끊으려고 close 호출. 이 시점에 recv 안 걸려 있으면 iocount가 1인 상태로 유지됨.
			// 이후 recv를 걸어버리면 세션이 살아있는 상태가 됨. 그래서 recv pending 이후 한 번 더 해보는 것.
			//recv 성공했다면 다시 걸 때 위에서 return 할 것. 
			if (_connect == false)
			{
				//하지만 ioPending 뜨고 나서 외부에서 연결 끊고, 해당 세션 release 타서 connect = false 된 상태에서 이 if문을 통과하고,
				//이후 연결된다면 잘못 된 disconnect가 생길 수 있다.
				//그래서 sessionID가지고 disconnect 시키는 것. 

				_owner->DisconnectSession(beforeSessionId);
			}

			return;
		}
	}
}

// IOCP
void Session::SetOwner(IOCP& owner)
{
	_owner = &owner;
}

// Timeout
void Session::SetDefaultTimeout(const int timeoutMillisecond)
{
	_defaultTimeout = timeoutMillisecond;
}

void Session::SetTimeout(const int timeoutMillisecond)
{
	_timeout = timeoutMillisecond;
}

void Session::ResetTimeoutWait()
{
	const auto now = std::chrono::steady_clock::now();
	lastRecv = now;
	_lastSend = now;
}

// Packet format
void Session::SetNetSession(const char staticKey)
{
	_staticKey = staticKey;
}

void Session::SetLanSession()
{
	SetNetSession(0);
}

void Session::SetMaxPacketLen(const int size)
{
	_maxPacketLen = size;
}

// Group
GroupID Session::GetGroupID() const
{
    return LoadState().GetGroupID();
}

void Session::CompleteMove()
{
    // 이동 중에 쌓인 데이터를 새 그룹에서 처리한다.
    if (const auto group = psh::RecvState::CompleteMove(_state))
        HandOffRecvTo(*group);
}

// Lifetime
long Session::GetRefCount(SessionID id)
{
	//auto value = _refCount;
	auto value = _refCount.load();
	ASSERT_CRASH(_sessionId == id, L"Session ID Wrong");
	return value;
}

// Send
void Session::RealSend()
{
	PRO_FUNCTION();


	const auto refIncResult = IncreaseRef(L"realSendInc");
	if (refIncResult >= RELEASE_FLAG)
	{
		Release(L"realSendSessionIsRelease");
		return;
	}

	size_t sendPackets = 0;
	SocketSendBuffer sendBuffers[MAX_SEND_COUNT] = {};

	volatile auto queueSize = _sendQ.Size();
	if (queueSize < 1)
	{
		psh::util::DebugBreak();
	}
	while (sendPackets == 0)
	{
		for (auto i = 0; i < MAX_SEND_COUNT; i++)
		{
			SendBufferImpl* buffer;
			volatile auto result = _sendQ.Dequeue(buffer);
			if (result == false)
			{
				break;
			}
			sendPackets++;

			_sendingQ[i] = buffer;

			if (_staticKey)
			{
				buffer->TryEncode(_staticKey);
			}
			else
			{
				buffer->WriteLanHeader();
			}

			const auto bufferSize = buffer->SendDataSize();
			SetSocketBuffer(sendBuffers[i], buffer->GetHead(), bufferSize);

			ASSERT_CRASH(bufferSize > 0, "Out of Case");
		}
	}

	_needCheckSendTimeout = true;
	_lastSend = std::chrono::steady_clock::now();

	_dataNotSend = sendPackets;

	const auto beforeSessionId = _sessionId;
	if (const auto sendResult = _socket.Send({sendBuffers, sendPackets});
		sendResult.HasError() && sendResult.Error() == NetErrc::Pending)
	{
		if (_connect == false)
		{
			_owner->DisconnectSession(beforeSessionId);
		}
	}
}

// Timeout
std::optional<timeoutInfo> Session::CheckTimeout(const std::chrono::steady_clock::time_point now)
{
	using namespace std::chrono;

	if (_timeout == 0)
	{
		return {};
	}
	if (_refCount >= RELEASE_FLAG)
	{
		return {};
	}


	if (IncreaseRef(L"timeoutInc") >= RELEASE_FLAG)
	{
		Release(L"TimeoutAlreadyRelease");
		return {};
	}

	const auto recvWait = duration_cast<milliseconds>(now - lastRecv);
	const auto sendWait = duration_cast<milliseconds>(now - _lastSend);

	std::optional<timeoutInfo> retVal{};
	if (_needCheckSendTimeout && sendWait.count() > _timeout)
	{
		retVal = {timeoutInfo::IOtype::send, sendWait.count(), _timeout};
	}
	else if (recvWait.count() > _timeout)
	{
		retVal = {timeoutInfo::IOtype::recv, recvWait.count(), _timeout};
	}

	Release(L"TimeoutRelease");
	return retVal;
}

// Debug
void Session::WriteContentLog(unsigned int type)
{
#ifdef SESSION_DEBUG
		Write(_refCount.load(), GroupID::InvalidGroupID(), L"SendContent");
#endif
}

// Receive
// group이 nullptr이면 기본 그룹(IOCP)에서 처리한다.
template <typename Header>
void Session::RecvHandler(Group* group)
{
	using enum psh::RecvState::Flag;

	const auto executingGroup = group ? group->GetGroupID() : GroupManager::BaseGroupID();
	Logger& logger = group ? *group->_groupLogger : *gLogger;
	int loopCount = 0;

	while (true)
	{
        const auto state = LoadState();
        if (state.Has(Moving) || state.GetGroupID() != executingGroup)
            break;

		if (_recvQ.Size() < sizeof(Header))
		{
			break;
		}

		Header header;
		_recvQ.Peek(reinterpret_cast<char*>(&header), sizeof(Header));

		if (header.len > _recvQ.GetBufferSize() - 1 - sizeof(Header))
        {
            logger.Write(L"Disconnect", Logger::LogLevel::Debug, L"LargePacket id : %d\n", GetSessionId());
            Close();
            break;
        }

		if constexpr (std::is_same_v<Header, NetHeader>)
		{
			if (header.code != dfPACKET_CODE)
			{
				int frontIndex = _recvQ.GetFrontIndex();
				logger.Write(L"Disconnect", Logger::LogLevel::Debug, L"WrongPacketCode id : %d\n index: %d \n"
				             , GetSessionId(), frontIndex);
				Close();
				break;
			}

			if (header.len > _maxPacketLen)
			{
				int frontIndex = _recvQ.GetFrontIndex();
				logger.Write(L"Disconnect", Logger::LogLevel::Debug, L"WrongPacketLen id : %d\n index: %d \n"
				             , GetSessionId(), frontIndex);
				Close();
				break;
			}
		}

		if (const int totPacketSize = header.len + sizeof(Header);
			_recvQ.Size() < totPacketSize)
		{
			break;
		}

		RecvBuffer buffer(&_recvQ, header.len);

		if constexpr (std::is_same_v<Header, NetHeader>)
		{
			_recvQ.Decode(_staticKey);

			if (!_recvQ.ChecksumValid())
			{
				logger.Write(L"Disconnect", Logger::LogLevel::Debug, L"Recv Invalid Checksum id : %d"
				             , GetSessionId());
				Close();
				break;
			}
		}
		loopCount++;
		_recvQ.Dequeue(sizeof(Header));
		if (group)
			group->onRecvPacket(*this, buffer);
		else
			_owner->onRecvPacket(*this, buffer);
	}

	if (loopCount > 0)
	{
		_owner->IncreaseRecvCount(loopCount);
	}
}

template void Session::RecvHandler<NetHeader>(Group* group);
template void Session::RecvHandler<LANHeader>(Group* group);
