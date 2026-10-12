#include "LinuxSocket.h"
#include "Socket.h"

#include <cerrno>
#include <utility>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <sys/socket.h>

namespace
{
    NetError Unsupported()
    {
        return NetError::FromNativeCode(ENOSYS);
    }
}

NetError NetError::FromNativeCode(const int nativeCode)
{
    return {nativeCode};
}

NetErrc NetError::ToNetErrc(const int nativeCode)
{
    switch (nativeCode)
    {
    case 0:
        return NetErrc::None;
    case EINPROGRESS:
    case EALREADY:
    case EAGAIN:
#if EWOULDBLOCK != EAGAIN
    case EWOULDBLOCK:
#endif
        return NetErrc::Pending;
    case EINTR:
        return NetErrc::Interrupted;
    case EINVAL:
        return NetErrc::InvalidParameter;
    case ECONNRESET:
    case EPIPE:
        return NetErrc::ConnectionReset;
    case ECONNABORTED:
        return NetErrc::ConnectionAborted;
    case ECANCELED:
        return NetErrc::OperationCanceled;
    case ETIMEDOUT:
        return NetErrc::TimedOut;
    case ENOENT:
        return NetErrc::NotFound;
    case ECONNREFUSED:
        return NetErrc::ConnectionRefused;
    case ENETUNREACH:
    case EHOSTUNREACH:
        return NetErrc::NetworkUnreachable;
    case EADDRINUSE:
        return NetErrc::AddressInUse;
    case EADDRNOTAVAIL:
        return NetErrc::AddressNotAvailable;
    case EACCES:
    case EPERM:
        return NetErrc::PermissionDenied;
    case ENOBUFS:
    case ENOMEM:
    case EMFILE:
    case ENFILE:
        return NetErrc::ResourceExhausted;
    case InvalidExecutorCode:
        return NetErrc::InvalidExecutor;
    case PartialSendCode:
        return NetErrc::PartialSend;
    default:
        return NetErrc::Unknown;
    }
}

psh::Result<NetError> Socket::InitPlatform()
{
    return true;
}

void Socket::CleanupPlatform()
{
}

/////////////////////
//      Socket
/////////////////////

Socket::Socket() : LinuxSocket(psh::InvalidHandle, {})
{
}

Socket::Socket(Executor& executor) : Socket(executor, psh::InvalidHandle, {})
{
}

Socket::Socket(Executor& executor, const psh::NativeHandle socket, const SockAddr address)
    : LinuxSocket(socket, address)
    , _executor(&executor)
    , _sendExecutable(std::make_unique<Executable>(Executable::IoType::Send))
    , _recvExecutable(std::make_unique<Executable>(Executable::IoType::Recv))
{
    // 콜백의 this 포인터만 설정하기 위해 bind를 사용한다.
    _sendExecutable->SetCallback(
        std::bind_front(&Socket::OnSendComplete, this));
}

Socket::~Socket()
{
    if (_registered)
    {
        Close();
    }
}

Socket::Socket(Socket&& other) noexcept
    : LinuxSocket(std::move(other))
    , _executor(std::exchange(other._executor, nullptr))
    , _registered(std::exchange(other._registered, false))
    , _beforeSocket(other._beforeSocket)
    , _sendExecutable(std::move(other._sendExecutable))
    , _recvExecutable(std::move(other._recvExecutable))
{
    other._socket = psh::InvalidHandle;
    if (_sendExecutable)
    {
        // 이동 후 콜백의 this 포인터만 수정하기 위해 bind를 사용한다.
        _sendExecutable->SetCallback(
            std::bind_front(&Socket::OnSendComplete, this));
    }
}

Socket& Socket::operator=(Socket&& other) noexcept
{
    if (this != &other)
    {
        if (_registered)
        {
            Close();
        }

        LinuxSocket::operator=(std::move(other));
        _executor = other._executor;
        _registered = other._registered;
        _beforeSocket = other._beforeSocket;
        _sendExecutable = std::move(other._sendExecutable);
        _recvExecutable = std::move(other._recvExecutable);

        other._socket = psh::InvalidHandle;
        other._executor = nullptr;
        other._registered = false;
        if (_sendExecutable)
        {
            // 이동 후 콜백의 this 포인터만 수정하기 위해 bind를 사용한다.
            _sendExecutable->SetCallback(
                std::bind_front(&Socket::OnSendComplete, this));
        }
    }
    return *this;
}

psh::Result<NetError> Socket::Init()
{
    _socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (_socket == psh::InvalidHandle)
    {
        return NetError::FromNativeCode(errno);
    }

    if (auto registerResult = RegisterToExecutor(); registerResult.HasError())
    {
        close(_socket);
        _socket = psh::InvalidHandle;
        return registerResult.Error();
    }
    return true;
}

psh::Result<NetError> Socket::Bind(Ip ip, Port port)
{
    _sockAddr = SockAddr(ip, port);

    auto sockAddrIn = _sockAddr.ToNative();

    if (bind(_socket, reinterpret_cast<const sockaddr*>(&sockAddrIn), sizeof(sockAddrIn)) == -1)
    {
        return NetError::FromNativeCode(errno);
    }

    return true;
}

psh::Result<NetError> Socket::Listen(int backlog) const
{
    if (listen(_socket, backlog) == -1)
    {
        return NetError::FromNativeCode(errno);
    }

    return true;
}

void Socket::StopListening()
{
    ::shutdown(_socket, SHUT_RDWR);
}

psh::Result<Socket, NetError> Socket::Accept() const
{
    if (_executor == nullptr)
        return NetError::FromNativeCode(NetError::InvalidExecutorCode);

    sockaddr_in clientAddr{};
    socklen_t len = sizeof(clientAddr);

    auto clientSocket = accept(_socket, reinterpret_cast<sockaddr*>(&clientAddr), &len);

    if (clientSocket == -1)
    {
        return NetError::FromNativeCode(errno);
    }

    auto accepted = Socket(*_executor, clientSocket, SockAddr(clientAddr));
    if (auto registerResult = accepted.RegisterToExecutor(); registerResult.HasError())
    {
        accepted.Close();
        return registerResult.Error();
    }

    return std::move(accepted);
}

psh::Result<NetError> Socket::Connect(Ip ip, Port port)
{
    if (_executor == nullptr)
        return NetError::FromNativeCode(NetError::InvalidExecutorCode);

    _sockAddr = SockAddr(ip, port);

    auto serverAddr = _sockAddr.ToNative();

    if (connect(_socket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == -1)
    {
        return NetError::FromNativeCode(errno);
    }

    return true;
}

psh::Result<NetError> Socket::CancelIO() const
{
    _executor->CancelIO(*this);
    return true;
}

psh::Result<NetError> Socket::RegisterToExecutor()
{
    if (_executor == nullptr)
    {
        return NetError::FromNativeCode(NetError::InvalidExecutorCode);
    }

    if (auto error = _executor->RegisterHandle(_socket);
        error != NetErrc::None)
    {
        return error;
    }

    _registered = true;
    return true;
}


psh::Result<NetError> Socket::Close()
{
    _beforeSocket = _socket;
    _socket = psh::InvalidHandle;
    const int closeResult = close(_beforeSocket);
    const int closeError = closeResult == -1 ? errno : 0;

    // 등록 해제는 close 성공 여부와 관계없이 한 번만 한다.
    if (_registered)
    {
        _registered = false;
        _executor->UnregisterHandle();
    }

    if (closeResult == -1)
    {
        return NetError::FromNativeCode(closeError);
    }
    return true;
}

bool Socket::IsValid() const
{
    return _socket != psh::InvalidHandle;
}

void Socket::SetCallbacks(IoCallback recvCallback, IoCallback sendCallback)
{
    if (_recvExecutable)
        _recvExecutable->SetCallback(std::move(recvCallback));
    if (_sendExecutable)
        _sendCallback = std::move(sendCallback);
}

void Socket::OnSendComplete(NetError error, std::size_t transferred)
{
    if (error != NetErrc::None)
    {
        if (_sendCallback)
            _sendCallback(error, transferred);
        return;
    }

    // MSG_WAITALL도 오류나 취소 시 원인 대신 부분 전송량을 반환할 수 있다.
    if (transferred < _sendState.requestedBytes)
    {
        _sendCallback(NetError::FromNativeCode(NetError::PartialSendCode), transferred);
        return;
    }

    _sendCallback(NetError{}, transferred);
}

psh::Result<NetError> Socket::Send(SocketSendBufferSpan buffers) const
{
    _sendExecutable->Clear();
    _sendState.Prepare(buffers);

    io_uring_sqe sqe{};
    io_uring_prep_sendmsg(&sqe, _socket, &_sendState.message, MSG_NOSIGNAL | MSG_WAITALL);

    io_uring_sqe_set_data(&sqe, _sendExecutable.get());
    _executor->ioUring_.RequestIo(sqe);

    return NetError::FromNativeCode(EINPROGRESS);
}

psh::Result<NetError> Socket::Recv(SocketRecvBufferSpan buffers) const
{
    _recvExecutable->Clear();
    //  int8 이상은 안 보내는 제한
    if (buffers.empty() || buffers.size() > (std::numeric_limits<psh::int8>::max)())
    {
        auto error = NetError::FromNativeCode(EINVAL);
        _recvExecutable->SetResult(error, 0);
        _executor->PostInternal(*_recvExecutable);
        return error;
    }

    _recvExecutable->Clear();
    _recvState.Prepare(buffers);

    io_uring_sqe sqe{};

    io_uring_prep_recvmsg(&sqe, _socket, &_recvState.message, 0);
    io_uring_sqe_set_data(&sqe, _recvExecutable.get());

    _executor->ioUring_.RequestIo(sqe);

    return true;
}

NetError Socket::LastError()
{
    return NetError::FromNativeCode(errno);
}

psh::Result<NetError> Socket::SetLinger(bool on) const
{
    linger option = {on ? 1 : 0, 0};
    if (setsockopt(_socket, SOL_SOCKET, SO_LINGER, &option, sizeof(option)) == -1)
    {
        return NetError::FromNativeCode(errno);
    }
    return true;
}

psh::Result<NetError> Socket::SetNoDelay(bool on) const
{
    int enabled = on ? 1 : 0;
    if (setsockopt(_socket, IPPROTO_TCP, TCP_NODELAY, &enabled, sizeof(enabled)) == -1)
    {
        return NetError::FromNativeCode(errno);
    }
    return true;
}

psh::Result<NetError> Socket::SetSendBuffer(int size) const
{
    int bufSize = size;
    if (setsockopt(_socket, SOL_SOCKET, SO_SNDBUF, &bufSize, sizeof(bufSize)) == -1)
    {
        return NetError::FromNativeCode(errno);
    }
    return true;
}


SockAddr Socket::GetSockAddress() const
{
    return _sockAddr;
}

Ip Socket::GetIp() const
{
    return _sockAddr.GetIp();
}

Port Socket::GetPort() const
{
    return _sockAddr.GetPort();
}
