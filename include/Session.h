#pragma once
#include <chrono>
#include <optional>

#include "RingBuffer.h"
#include "GroupTypes.h"
#include "LockFreeFixedQueue.h"
#include "Macro.h"
#include "SessionTypes.h"
#include "Socket.h"
#include "Types.h"

class SendBufferImpl;
class RecvBuffer;
class IOCP;

class RecvExecutable;
class SendExecutable;
class PostSendExecutable;
class ReleaseExecutable;

//#define SESSION_DEBUG
class Session
{
    friend class RecvExecutable;
    friend class PostSendExecutable;
    friend class SendExecutable;
    friend class ReleaseExecutable;
    friend class IOCP;
    friend class Group;

public:
    Session();
    Session(Socket socket, SessionID sessionId, IOCP& owner);

    Session(const Session &other) = delete;

    Session(Session &&other) noexcept = delete;

    Session & operator=(const Session &other) = delete;

    Session & operator=(Session &&other) noexcept = delete;

    void Close();
    void Reset();
    void PostRelease();


    inline long IncreaseRef(psh::LPCWSTR content);
    bool Release(psh::LPCWSTR content = L"", int type = 0);


    void EnqueueSendData(SendBufferImpl* buffer);
    void RegisterRecv();
    void RecvNotIncrease();
    void TrySend();
    bool CanSend();


    void RegisterIOCP(HANDLE iocpHandle);

    SessionID GetSessionId() const;

    String GetIp() const;

    psh::uint16 GetPort() const;

    void SetSocket(const Socket& socket);

    void SetSessionId(const SessionID sessionID);

    void SetErr(const int errCode);

    int GetErr();

    void SetOwner(IOCP& owner);

    void SetNetSession(const char staticKey);

    void SetLanSession();

    void SetMaxPacketLen(const int size);

    void SetConnect();

    void SetDefaultTimeout(int timeoutMillisecond);
    void SetTimeout(int timeoutMillisecond);

    void OffReleaseFlag();

    void ResetTimeoutWait();

    //0이면 아무 그룹 아님. 
    GroupID GetGroupID() const;

    void SetGroupID(const GroupID id);

private:
    std::optional<timeoutInfo> CheckTimeout(std::chrono::steady_clock::time_point now);
    void RealSend();

private:
    long GetRefCount(SessionID id);

    //CONST
    static constexpr unsigned long long ID_MASK = 0x000'7FFF'FFFF'FFFF;
    static constexpr unsigned long long INDEX_MASK = 0x7FFF'8000'0000'0000;
    static constexpr int MAX_SEND_COUNT = 512;

    static constexpr long RELEASE_FLAG = 0x0010'0000;


    //Network	
    alignas(64) SessionID _sessionId = {{0}};
    Socket _socket;
    GroupID _groupId = GroupID(0);
    long _refCount = RELEASE_FLAG;
    volatile int _errCode = -1;
    
#ifdef SESSION_DEBUG
	long DebugRef = 0;
#endif
    char _connect = false;

    LockFreeFixedQueue<SendBufferImpl*, MAX_SEND_COUNT> _sendQ;
    //TlsLockFreeQueue<CSendBuffer*> _sendQ;
    SendBufferImpl* _sendingQ[MAX_SEND_COUNT];


    //TlsLockFreeQueue<RecvBuffer*> _groupRecvQ;

    IOCP* _owner;


    //Executable
    RecvExecutable& _recvExecute;
    PostSendExecutable& _postSendExecute;
    SendExecutable& _sendExecute;
    ReleaseExecutable& _releaseExecutable;


    //Reference
    char _isSending = false;

    //Timeout
    bool _needCheckSendTimeout = false;

    int _defaultTimeout = 5000;
    int _timeout = 5000;
    RingBuffer _recvQ;
    std::chrono::steady_clock::time_point lastRecv;
    
    //Encrypt
    char _staticKey = false;
    unsigned int _maxPacketLen = 5000;

    //DEBUG
    long debugCount = 0;
#ifdef SESSION_DEBUG

	struct RelastinReleaseEncrypt_D
	{
		SessionID id;
		GroupID dst = GroupID::InvalidGroupID();
		long refCount;
		psh::LPCWSTR location;
		GroupID groupId = GroupID::InvalidGroupID();
	};
	static const int debugSize = 2000;

	long debugIndex = 0;
	RelastinReleaseEncrypt_D release_D[debugSize];

	//세션 id, 종류
	//tuple<SessionID, String, thread::id> _debugLeave[debugSize];
	public:
	void Write(long refCount, GroupID dst, psh::LPCWSTR cause)
	{
		auto index = InterlockedIncrement(&debugIndex);
		release_D[index % debugSize] = { GetSessionId(),dst,refCount, cause,GetGroupID()};
	}

#endif


    void WriteContentLog(unsigned int type);
};


long Session::IncreaseRef(psh::LPCWSTR content)
{
    auto result = InterlockedIncrement(&_refCount);

#ifdef SESSION_DEBUG
	Write(_refCount, GroupID::InvalidGroupID(), content);
#else
    UNREFERENCED_PARAMETER(content);
#endif
    return result;
}
