#include "Session.h"
#include <optional>

#include "SendBuffer.h"
#include "IOCP.h"

#include "Logger.h"
#include "CoreGlobal.h"
#include "RecvBuffer.h"
#include "Executables.h"

Session::Session()
    : _sessionId()
    , _socket({})
    , _sendingQ()
    , _owner(nullptr)
    , _recvExecute(*new RecvExecutable)
    , _postSendExecute(*new PostSendExecutable)
    , _sendExecute(*new SendExecutable)
    , _releaseExecutable(*new ReleaseExecutable)
{
}

Session::Session(Socket socket, SessionID sessionId, IOCP& owner)
    : _sessionId(sessionId)
    , _socket(socket)
    , _sendingQ()
    , _owner(&owner)
    , _recvExecute(*new RecvExecutable)
    , _postSendExecute(*new PostSendExecutable)
    , _sendExecute(*new SendExecutable)
    , _releaseExecutable(*new ReleaseExecutable)
{
}

bool Session::CanSend()
{
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
        if (InterlockedExchange8(&_isSending, true) == 1)
        {
            return false;
        }

        if (_sendQ.Size() == 0)
        {
            InterlockedExchange8(&_isSending, false);

            continue;
        }
        break;
    }
    return true;
}

void Session::EnqueueSendData(SendBufferImpl* buffer)
{
    _sendQ.Enqueue(buffer);
}

bool Session::Release(psh::LPCWSTR content, int type)
{
    int refDecResult = InterlockedDecrement(&_refCount);

#ifdef SESSION_DEBUG
	Write(_refCount, GroupID::InvalidGroupID(), content);
#else
    UNREFERENCED_PARAMETER(content);
    UNREFERENCED_PARAMETER(type);
#endif

    if (refDecResult == 0)
    {
        if (InterlockedCompareExchange(&_refCount, RELEASE_FLAG, 0) == 0)
        {
            //InterlockedIncrement(&_owner->_iocpCompBufferSize);

            //InterlockedOr((long*)&(_groupId), 0) == 0

            if (GetGroupID() == GroupManager::BaseGroupID())
            {
#ifdef SESSION_DEBUG
				Write(_refCount, GroupID::InvalidGroupID(), L"SessionGroup 0 PostRelease");
#endif
                PostRelease();
            }
            else
            {
#ifdef SESSION_DEBUG
				Write(_refCount, GroupID::InvalidGroupID(), L"SessionGroup other");
#endif
                _owner->_groupManager->SessionDisconnected(this);
            }
            return true;
        }
    }
    return false;
}

void Session::Close()
{
    InterlockedExchange8(&_connect, 0);
    _socket.CancelIO();
}


void Session::TrySend()
{
    if (!CanSend())
    {
        return;
    }

    RealSend();
}

void Session::RegisterRecv()
{
    InterlockedIncrement(&_refCount);

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

    _recvExecute._recvBufSize[0] = 0;
    _recvExecute._recvBufSize[1] = 0;
    _recvExecute.isPending = false;

    for (int i = 0; i < bufferCount; i++)
    {
        _recvExecute._recvBufSize[i] = recvWsaBuf[i].len;
    }
    ASSERT_CRASH(_recvExecute._recvBufSize[0] + _recvExecute._recvBufSize[1] != 0, "RecvBuffer is 0");

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
            _recvExecute.isPending = true;
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


void Session::RegisterIOCP(const HANDLE iocpHandle)
{
    CreateIoCompletionPort(reinterpret_cast<HANDLE>(_socket._socket), iocpHandle, (ULONG_PTR)this, 0);
}

SessionID Session::GetSessionId() const
{
    return _sessionId;
}

String Session::GetIp() const
{
    return _socket.GetIp();
}

psh::uint16 Session::GetPort() const
{
    return _socket.GetPort();
}

void Session::SetSocket(const Socket &socket)
{
    _socket = socket;
}

void Session::SetSessionId(const SessionID sessionID)
{
    _sessionId = sessionID;
}

void Session::SetErr(const int errCode)
{
    _errCode = errCode;
}

int Session::GetErr()
{
    return _errCode;
}

void Session::SetOwner(IOCP &owner)
{
    _owner = &owner;
}

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

void Session::SetConnect()
{
    _connect = true;
}

void Session::OffReleaseFlag()
{
    InterlockedBitTestAndReset(&_refCount, 20);
}

GroupID Session::GetGroupID() const
{
    return _groupId;
}

void Session::SetGroupID(const GroupID id)
{
    InterlockedExchange((long*)&_groupId, long(id));
}

long Session::GetRefCount(SessionID id)
{
    //auto value = _refCount;
    auto value = InterlockedOr(&_refCount, 0);
    ASSERT_CRASH(_sessionId == id, L"Session ID Wrong");
    return value;
}

void Session::WriteContentLog(unsigned int type)
{
#ifdef SESSION_DEBUG
		Write(_refCount, GroupID::InvalidGroupID(), L"SendContent");
#else
    UNREFERENCED_PARAMETER(type);
#endif
}

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
    _postSendExecute.lastSend = now;
}


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
    const auto sendWait = duration_cast<milliseconds>(now - _postSendExecute.lastSend);

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

    const int notSend = InterlockedExchange(&_postSendExecute.dataNotSend, 0);

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

void Session::RealSend()
{
    const auto refIncResult = IncreaseRef(L"realSendInc");
    if (refIncResult >= RELEASE_FLAG)
    {
        Release(L"realSendSessionIsRelease");
        return;
    }

    int sendPackets = 0;
    WSABUF sendWsaBuf[MAX_SEND_COUNT] = {};

    while (sendPackets == 0)
    {
        for (int i = 0; i < MAX_SEND_COUNT; i++)
        {
            SendBufferImpl* buffer;
            if (!_sendQ.Dequeue(buffer))
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
    _postSendExecute.lastSend = std::chrono::steady_clock::now();

    constexpr DWORD flags = 0;
    _postSendExecute.Clear();

    _postSendExecute.dataNotSend = sendPackets;

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
