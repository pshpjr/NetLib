#include "Profiler.h"
#include "Session.h"

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
	  , _recvExecute(*new Executable(Executable::IoType::Recv))
	  , _postSendExecute(*new Executable(Executable::IoType::Postrecv))
	  , _sendExecute(*new Executable(Executable::IoType::Send))
	  , _releaseExecutable(*new Executable(Executable::IoType::Release))
{
	_recvExecute.SetCallback([this](ULONG_PTR, DWORD transferred, void* iocp)
	{
		_recvQ.MoveRear(transferred);
		auto& server = *static_cast<IOCP*>(iocp);
		if (GetGroupID() != GroupManager::BaseGroupID() && !recvHandleWaitInGroup())
		{
			setRecvHandled(true);
			server._groupManager->OnRecv(GetSessionId(), GetGroupID(), transferred);
		}
		else if (_staticKey)
		{
			RecvHandler<NetHeader>(iocp);
		}
		else
		{
			RecvHandler<LANHeader>(iocp);
		}
		RecvNotIncrease();
	});

	_postSendExecute.SetCallback([this](ULONG_PTR, DWORD, void* iocp)
	{
		auto& server = *static_cast<IOCP*>(iocp);
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

	_sendExecute.SetCallback([this](ULONG_PTR, DWORD, void*)
	{
		RealSend();
	});

	_releaseExecutable.SetCallback([this](ULONG_PTR, DWORD, void*)
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
    _socket = socket;
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
String Session::GetIp() const
{
	return _socket.GetIp();
}

psh::uint16 Session::GetPort() const
{
	return _socket.GetPort();
}

void Session::SetSocket(const Socket& socket)
{
	_socket = socket;
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
	_errCode = errCode;
}

int Session::GetErr()
{
	return _errCode;
}

// Lifetime
void Session::Reset()
{
	//debugIndex = 0;
	_sendExecute.Clear();
	_recvExecute.Clear();
	_postSendExecute.Clear();
	_recvQ.Clear();
	_isSending = false;
	_needCheckSendTimeout = false;
	_timeout = _defaultTimeout;
	_errCode = -1;

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
	SetGroupID(GroupID(0));
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
	PostQueuedCompletionStatus(_owner->_iocp, static_cast<DWORD>(-1), reinterpret_cast<ULONG_PTR>(this)
	                           , _releaseExecutable.GetOverlapped());
}

bool Session::Release(psh::LPCWSTR content, int type)
{
	int refDecResult = --_refCount;

#ifdef SESSION_DEBUG
	Write(_refCount.load(), GroupID::InvalidGroupID(), content);
#else
	UNREFERENCED_PARAMETER(content);
	UNREFERENCED_PARAMETER(type);
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

// Send
bool Session::EnqueueSendData(SendBufferImpl* buffer)
{
	return _sendQ.Enqueue(buffer);
}

void Session::TrySend()
{
	if (!CanSend())
	{
		return;
	}

	RealSend();
}

bool Session::CanSend()
{
	PRO_FUNCTION();
	if (_connect == false)
	{
		return false;
	}
	while (true)
	{
		//이걸로 disconnect시 전송 을 방지할 수 있을까?
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

// Receive
void Session::RegisterRecv()
{
	++_refCount;

	RecvNotIncrease();
}

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
	int bufferCount = 1;
	WSABUF recvWsaBuf[2] = {};
	DWORD flags = 0;

	recvWsaBuf[0].buf = _recvQ.GetRear();
	recvWsaBuf[0].len = _recvQ.DirectEnqueueSize();

	if (_recvQ.FreeSize() > recvWsaBuf[0].len)
	{
		recvWsaBuf[1].buf = _recvQ.GetBuffer();
		recvWsaBuf[1].len = _recvQ.FreeSize() - recvWsaBuf[0].len;

		bufferCount = 2;
	}
	_recvExecute.Clear();

	ASSERT_CRASH(recvWsaBuf[0].len + recvWsaBuf[1].len != 0, "RecvBuffer is 0");

	lastRecv = std::chrono::steady_clock::now();


	auto beforeSessionId = _sessionId;
	if (const int recvResult = _socket.Recv(recvWsaBuf, bufferCount, &flags, _recvExecute.GetOverlapped());
		recvResult == SOCKET_ERROR)
	{
		const int error = WSAGetLastError();
		if (error == WSA_IO_PENDING)
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
		SetErr(error);
		switch (error)
		{
		case WSAECONNABORTED:
			__fallthrough;

		//원격지에서 끊었다. 
		case WSAECONNRESET:
			__fallthrough;
			break;
		default:
			{
				__debugbreak();
			}
		}
		Release(L"RecvErrorRelease");
	}
}

// IOCP
void Session::RegisterIOCP(const HANDLE iocpHandle)
{
	CreateIoCompletionPort(reinterpret_cast<HANDLE>(_socket._socket), iocpHandle, (ULONG_PTR)this, 0);
}

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
	return _groupId.load();
}

void Session::SetGroupID(const GroupID id)
{
	_groupId.store(id);
}

bool Session::recvHandleWaitInGroup()
{
	return _isHandled.load(std::memory_order::acquire);
}

void Session::setRecvHandled(bool value)
{
	_isHandled.store(value, std::memory_order::release);
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

	int sendPackets = 0;
	WSABUF sendWsaBuf[MAX_SEND_COUNT] = {};

	volatile auto queueSize = _sendQ.Size();
	if (queueSize < 1)
	{
		__debugbreak();
	}
	while (sendPackets == 0)
	{
		for (int i = 0; i < MAX_SEND_COUNT; i++)
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

			sendWsaBuf[i].buf = buffer->GetHead();
			sendWsaBuf[i].len = buffer->SendDataSize();

			ASSERT_CRASH(sendWsaBuf[i].len > 0, "Out of Case");
		}
	}
	int sendSize = 0;
	for (int i = 0; i < sendPackets; i++)
	{
		sendSize += sendWsaBuf[i].len;
	}

	_needCheckSendTimeout = true;
	_lastSend = std::chrono::steady_clock::now();

	constexpr DWORD flags = 0;
	_postSendExecute.Clear();

	_dataNotSend = sendPackets;

	if (const int sendResult = _socket.Send(sendWsaBuf, sendPackets, flags, _postSendExecute.GetOverlapped());
		sendResult == SOCKET_ERROR)
	{
		const int error = WSAGetLastError();
		if (error == WSA_IO_PENDING)
		{
			return;
		}
		SetErr(error);
		switch (error)
		{
		case WSAECONNABORTED:
			__fallthrough;
		//원격지에서 끊었다. 
		case WSAECONNRESET:
			__fallthrough;
			break;
		default:
			gLogger->Write(L"Disconnect", Logger::LogLevel::Debug, L"SendFail errNo : %d, sessionID : %d", error
			               , GetSessionId());
			__debugbreak();
		}
		Release(L"SendErrorRelease");
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
#else
	UNREFERENCED_PARAMETER(type);
#endif
}

// Receive
template <typename Header>
void Session::RecvHandler(void* iocp)
{
	Session& session = *this;
	IOCP& server = *static_cast<IOCP*>(iocp);
	int loopCount = 0;

	RingBuffer& recvQ = session._recvQ;
	while (true)
	{
		if (session.GetGroupID() != GroupManager::BaseGroupID())
		{
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
				gLogger->Write(L"Disconnect", Logger::LogLevel::Debug, L"WrongPacketCode id : %d\n index: %d \n"
				               , session.GetSessionId(), frontIndex);
				session.Close();
				break;
			}

			if (header.len > session._maxPacketLen)
			{
				int frontIndex = recvQ.GetFrontIndex();
				gLogger->Write(L"Disconnect", Logger::LogLevel::Debug, L"WrongPacketLen id : %d\n index: %d \n"
				               , session.GetSessionId(), frontIndex);
				session.Close();
				break;
			}
		}

		if (header.len > recvQ.Size())
		{
			break;
		}


		if (const int totPacketSize = header.len + sizeof(Header);
			recvQ.Size() < totPacketSize)
		{
			break;
		}

		RecvBuffer buffer(&recvQ, header.len);
		//auto& buffer = *RecvBuffer::Alloc(&recvQ,header.len);

		if constexpr (std::is_same_v<Header, NetHeader>)
		{
			recvQ.Decode(session._staticKey);

			if (!recvQ.ChecksumValid())
			{
				gLogger->Write(L"Disconnect", Logger::LogLevel::Debug, L"Recv Invalid Checksum id : %d"
				               , session.GetSessionId());
				session.Close();
				break;
			}
		}
		loopCount++;
		recvQ.Dequeue(sizeof(Header));
		server.onRecvPacket(session, buffer);

		ASSERT_CRASH(buffer.CanPopSize() == 0, "UnuseData");
	}

	if (loopCount > 0)
	{
		server.IncreaseRecvCount(loopCount);
	}
}

template void Session::RecvHandler<NetHeader>(void* iocp);
template void Session::RecvHandler<LANHeader>(void* iocp);
