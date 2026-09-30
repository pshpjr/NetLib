#pragma once
#include <LockFreeFixedQueue.h>

#include <atomic>
#include <chrono>
#ifdef NETLIB_USE_SCQ_SEND_QUEUE
#include "SCQ.h"
#endif
#include "GroupTypes.h"
#include "LockQueue.h"
#include "Macro.h"
#include "RingBuffer.h"
#include "SessionTypes.h"
#include "Socket.h"
#include "Types.h"

#include <optional>

class SendBufferImpl;
class RecvBuffer;
class IOCP;
class Executable;


//#define SESSION_DEBUG
class Session
{
	static constexpr unsigned long long ID_MASK = 0x000'7FFF'FFFF'FFFF;
	static constexpr unsigned long long INDEX_MASK = 0x7FFF'8000'0000'0000;
	static constexpr long RELEASE_FLAG = 0x0010'0000;
	static constexpr int MAX_SEND_COUNT = 512;

#ifdef SESSION_DEBUG
	static const int debugSize = 2000;
#endif

	friend class IOCP;
	friend class Group;

public:
	// Construction
	Session();
	Session(Socket socket, SessionID sessionId, IOCP& owner);
	Session(const Session& other) = delete;
	Session(Session&& other) noexcept = delete;
	Session& operator=(const Session& other) = delete;
	Session& operator=(Session&& other) noexcept = delete;

	// Identity
	SessionID GetSessionId() const;
	void SetSessionId(const SessionID sessionID);

	// Connection
	String GetIp() const;
	psh::uint16 GetPort() const;
	//  TODO: 범용 주소 타입 만들면 교체
	SOCKADDR_IN GetAddress() const;
	void SetSocket(Socket&& socket);
	void SetConnect();
	void Close();
	void SetErr(const int errCode);
	void SetErr(NetError error);
	int  GetErr();

	// Lifetime
	void Reset();
	void PostRelease();
	inline long IncreaseRef(psh::LPCWSTR content);
	bool Release(psh::LPCWSTR content = L"", int type = 0);
	void OffReleaseFlag();

	// Send
	bool EnqueueSendData(SendBufferImpl* buffer);
	void TrySend();
	bool CanSend();

	// Receive
	void RegisterRecv();
	void RecvNotIncrease();

	// IOCP
	void SetOwner(IOCP& owner);

	// Timeout
	void SetDefaultTimeout(int timeoutMillisecond);
	void SetTimeout(int timeoutMillisecond);
	void ResetTimeoutWait();

	// Packet format
	void SetNetSession(const char staticKey);
	void SetLanSession();
	void SetMaxPacketLen(const int size);

	// Group
	//0이면 아무 그룹 아님.
	GroupID GetGroupID() const;
	void SetGroupID(const GroupID id);
	bool recvHandleWaitInGroup();
	void setRecvHandled(bool value);

#ifdef SESSION_DEBUG
	void Write(long refCount, GroupID dst, psh::LPCWSTR cause)
	{
		auto index = ++debugIndex;
		release_D[index % debugSize] = { GetSessionId(),dst,refCount, cause,GetGroupID()};
	}
	void WriteContentLog(unsigned int type);
#endif

private:

#ifdef SESSION_DEBUG
	struct RelastinReleaseEncrypt_D
	{
		SessionID id;
		GroupID dst = GroupID::InvalidGroupID();
		long refCount;
		psh::LPCWSTR location;
		GroupID groupId = GroupID::InvalidGroupID();
	};
#endif

	// Lifetime
	long GetRefCount(SessionID id);

	// Send
	void RealSend();

	// Receive
	template <typename Header>
	void RecvHandler(void* iocp);

	// Timeout
	std::optional<timeoutInfo> CheckTimeout(std::chrono::steady_clock::time_point now);

#ifndef SESSION_DEBUG
	void WriteContentLog(unsigned int type);
#endif

	// Identity
	alignas(64) SessionID _sessionId = {{0}};

	// Connection
	Socket _socket;
	std::atomic_bool _connect = false;
	NetError _errCode{};

	// Lifetime
	std::atomic<long> _refCount = RELEASE_FLAG;
#ifdef SESSION_DEBUG
	long DebugRef = 0;
#endif

	// Send
#ifdef NETLIB_USE_SCQ_SEND_QUEUE
	SCQ<SendBufferImpl*, MAX_SEND_COUNT> _sendQ{};
#else
	LockFreeFixedQueue<SendBufferImpl*, MAX_SEND_COUNT> _sendQ{};
#endif
	//TlsLockFreeQueue<SendBufferImpl*> _sendQ;
	SendBufferImpl* _sendingQ[MAX_SEND_COUNT];
	std::atomic<bool> _isSending = false;
	std::atomic<long> _dataNotSend = 0;

	// IOCP dispatch
	IOCP* _owner;
	Executable& _recvExecute;
	Executable& _postSendExecute;
	// Executable& _sendExecute;
	Executable& _releaseExecutable;

	// Receive
	RingBuffer _recvQ;

	// Timeout
	bool _needCheckSendTimeout = false;
	int _defaultTimeout = 5000;
	int _timeout = 5000;
	std::chrono::steady_clock::time_point lastRecv;
	std::chrono::steady_clock::time_point _lastSend{};

	// Packet format
	char _staticKey = false;
	unsigned int _maxPacketLen = 5000;

	// Group
	std::atomic<GroupID> _groupId = GroupID(0);
	std::atomic_bool _isHandled = false;

	// Debug
	volatile std::atomic<int> _uniqueAccess;
	long debugCount = 0;
#ifdef SESSION_DEBUG

	std::atomic<long> debugIndex = 0;
	RelastinReleaseEncrypt_D release_D[debugSize];
	//세션 id, 종류
	//tuple<SessionID, String, thread::id> _debugLeave[debugSize];

#endif
};


long Session::IncreaseRef(psh::LPCWSTR content)
{
	auto result = ++_refCount;

#ifdef SESSION_DEBUG
	Write(_refCount.load(), GroupID::InvalidGroupID(), content);
#else
	UNREFERENCED_PARAMETER(content);
#endif
	return result;
}
