#include "NormalSocket.h"

#include "Executable.h"
#include "Macro.h"
#include "Socket.h"

#include <WS2tcpip.h>
#include <limits>

NetError NetError::FromNativeCode(const int nativeCode)
{
    return {nativeCode};
}

NetErrc NetError::ToNetErrc(int nativeCode)
{
    switch (nativeCode)
    {
    case 0:
        return NetErrc::None;
    case WSA_IO_PENDING:
        return NetErrc::Pending;
    case WSAEINTR:
        return NetErrc::Interrupted;
    case WSAECONNRESET:
    case ERROR_NETNAME_DELETED:
        return NetErrc::ConnectionReset;
    case WSAECONNABORTED:
    case ERROR_CONNECTION_ABORTED:
        return NetErrc::ConnectionAborted;
    case WSA_OPERATION_ABORTED:
        return NetErrc::OperationCanceled;
    case WSAETIMEDOUT:
    case ERROR_SEM_TIMEOUT:
        return NetErrc::TimedOut;
    case ERROR_NOT_FOUND:
        return NetErrc::NotFound;
    case WSAECONNREFUSED:
        return NetErrc::ConnectionRefused;
    case WSAENETUNREACH:
        return NetErrc::NetworkUnreachable;
    case WSAEADDRINUSE:
        return NetErrc::AddressInUse;
    case WSAEADDRNOTAVAIL:
        return NetErrc::AddressNotAvailable;
    case WSAEACCES:
        return NetErrc::PermissionDenied;
    case WSAENOBUFS:
    case WSAEMFILE:
        return NetErrc::ResourceExhausted;
    case NetError::InvalidExecutorCode:
        return NetErrc::InvalidExecutor;
    default:
        return NetErrc::Unknown;
    }
}

Socket::Socket()
    : NormalSocket(InvalidSocket, {0})
{
}


Socket::Socket(Executor& executor)
    : Socket(executor, InvalidSocket, {0})
{
}

Socket::Socket(Executor& executor, const NativeHandle socket, const SOCKADDR_IN address)
    : NormalSocket(socket, address)
    , _executor(&executor)
{
}

Socket::~Socket()
{
    if (_registered)
    {
        Close();
    }
}

Socket::Socket(Socket&& other) noexcept
    : NormalSocket(other)
    , _executor(other._executor)
    , _registered(other._registered)
    , _beforeSocket(other._beforeSocket)
{
    other._socket = InvalidSocket;
    other._executor = nullptr;
    other._registered = false;
}

Socket& Socket::operator=(Socket&& other) noexcept
{
    if (this != &other)
    {
        if (_registered)
        {
            Close();
        }
        NormalSocket::operator=(other);
        _executor = other._executor;
        _registered = other._registered;
        _beforeSocket = other._beforeSocket;

        other._socket = InvalidSocket;
        other._executor = nullptr;
        other._registered = false;
    }
    return *this;
}

psh::Result<NetError> Socket::RegisterToExecutor()
{
    if (_executor == nullptr)
        return NetError::FromNativeCode(NetError::InvalidExecutorCode);

    if (auto error = _executor->RegisterHandle(reinterpret_cast<HANDLE>(_socket));
        error != NetErrc::None)
        return error;

    _registered = true;
    return true;
}

bool Socket::IsSameSubnet(const String& comp, char subnetMaskBits) const
{
    IN_ADDR compAddress;
    InetPtonW(AF_INET, comp.c_str(), &compAddress);

    return IsSameSubnet(compAddress, _sockAddr.sin_addr, subnetMaskBits);
}

bool Socket::IsSameSubnet(const IN_ADDR& a, const IN_ADDR& b, const char subnetMaskBits)
{
    ASSERT_CRASH(0 <= subnetMaskBits && subnetMaskBits <= 32, "subnetSizeError");
    const unsigned long subnetMask = 0xFFFFFFFFull >> (32 - subnetMaskBits & 0xFFFFFFFF);

    const unsigned long compMasked = a.S_un.S_addr & subnetMask;
    const unsigned long tarMasked = b.S_un.S_addr & subnetMask;


    return compMasked == tarMasked;
}

psh::Result<NetError> Socket::CancelIO() const
{
    if (CancelIoEx(reinterpret_cast<HANDLE>(_socket), nullptr) == 0)
    {
        const auto error = NetError::FromNativeCode(GetLastError());
        // 취소할 I/O가 없는 경우는 성공으로 처리한다.
        if (error != NetErrc::NotFound)
        {
            return error;
        }
    }
    return true;
}

psh::Result<NetError> Socket::Close()
{
    _beforeSocket = _socket;
    _socket = InvalidSocket;
    const int closeResult = closesocket(_beforeSocket);
    const int closeError = closeResult == SOCKET_ERROR ? WSAGetLastError() : 0;

    //  등록 해제는 closesocket 성공 여부와 관계없이 한 번만 한다.
    if (_registered)
    {
        _registered = false;
        _executor->UnregisterHandle();
    }

    if (closeResult == SOCKET_ERROR)
    {
        return NetError::FromNativeCode(closeError);
    }
    return true;
}

psh::Result<NetError> Socket::Send(SocketSendBufferSpan buffers, Executable& executable) const
{
    executable.Clear();
    if (buffers.empty() || buffers.size() > (std::numeric_limits<DWORD>::max)())
    {
        const auto error = NetError::FromNativeCode(WSAEINVAL);
        executable.Execute(error, 0);
        return error;
    }
    if (WSASend(_socket, buffers.data(), static_cast<DWORD>(buffers.size()), nullptr, 0,
                executable.GetOverlapped(), nullptr) == SOCKET_ERROR)
    {
        auto error = NetError::FromNativeCode(WSAGetLastError());
        if (error == NetErrc::Pending)
            return error;

        _executor->PostInternal(executable);
        return error;
    }
    return true;
}

psh::Result<NetError> Socket::Recv(SocketRecvBufferSpan buffers, Executable& executable) const
{
    DWORD flags = 0;
    executable.Clear();
    if (buffers.empty() || buffers.size() > (std::numeric_limits<DWORD>::max)())
    {
        auto error = NetError::FromNativeCode(WSAEINVAL);
        executable.Execute(error, 0);
        return error;
    }
    if (WSARecv(_socket, buffers.data(), static_cast<DWORD>(buffers.size()), nullptr, &flags,
                executable.GetOverlapped(), nullptr) == SOCKET_ERROR)
    {
        const auto error = NetError::FromNativeCode(WSAGetLastError());
        if (error == NetErrc::Pending)
            return error;

        _executor->PostInternal(executable);
        return error;
    }
    return true;
}


NetError Socket::LastError()
{
    return NetError::FromNativeCode(WSAGetLastError());
}

psh::Result<NetError> Socket::SetLinger(const bool on) const
{
    linger l = {on, 0};
    if (setsockopt(_socket, SOL_SOCKET, SO_LINGER, (char*)&l, sizeof(l)) == SOCKET_ERROR)
    {
        return NetError::FromNativeCode(WSAGetLastError());
    }
    return true;
}

psh::Result<NetError> Socket::SetNoDelay(bool on) const
{
    if (setsockopt(_socket, IPPROTO_TCP, TCP_NODELAY, (char*)&on, sizeof(on)) == SOCKET_ERROR)
    {
        return NetError::FromNativeCode(WSAGetLastError());
    }
    return true;
}

psh::Result<NetError> Socket::SetSendBuffer(const int size) const
{
    int bufSize = size;
    if (setsockopt(_socket, SOL_SOCKET, SO_SNDBUF, (char*)&bufSize, sizeof(bufSize)) == SOCKET_ERROR)
    {
        return NetError::FromNativeCode(WSAGetLastError());
    }
    return true;
}


SOCKADDR_IN Socket::GetSockAddress() const
{
    return _sockAddr;
}

String Socket::GetIp() const
{
    WCHAR str[47];
    InetNtopW(AF_INET, &_sockAddr.sin_addr, str, 47);

    return String(str);
}

psh::uint16 Socket::GetPort() const
{
    psh::uint16 result;
    WSANtohs(_socket, _sockAddr.sin_port, &result);
    return result;
}

psh::Result<NetError> Socket::Init()
{
    _socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (_socket == InvalidSocket)
    {
        return NetError::FromNativeCode(WSAGetLastError());
    }

    if (auto registerResult = RegisterToExecutor(); registerResult.HasError())
    {
        closesocket(_socket);
        _socket = InvalidSocket;
        return registerResult.Error();
    }
    return true;
}

bool Socket::IsValid() const
{
    return _socket != InvalidSocket;
}


psh::Result<NetError> Socket::Bind(const String& ip, psh::uint16 port)
{
    memset(&_sockAddr, 0, sizeof(_sockAddr));
    _sockAddr.sin_family = AF_INET;
    _sockAddr.sin_addr = Ip2Address(ip.c_str());
    _sockAddr.sin_port = htons(port);

    if (bind(_socket, reinterpret_cast<SOCKADDR*>(&_sockAddr), sizeof(_sockAddr)) == SOCKET_ERROR)
    {
        return NetError::FromNativeCode(WSAGetLastError());
    }
    return true;
}

psh::Result<NetError> Socket::Listen(const int backlog) const
{
    if (listen(_socket, backlog) == SOCKET_ERROR)
    {
        return NetError::FromNativeCode(WSAGetLastError());
    }
    return true;
}

psh::Result<Socket, NetError> Socket::Accept() const
{
    SOCKADDR_IN clientAddr = {};
    int len = sizeof(SOCKADDR);

    NativeHandle clientSocket = accept(_socket, (SOCKADDR*)&clientAddr, &len);
    if (clientSocket == InvalidSocket)
    {
        return NetError::FromNativeCode(WSAGetLastError());
    }

    auto accepted = Socket(*_executor, clientSocket, clientAddr);
    accepted._executor = _executor;
    if (auto registerResult = accepted.RegisterToExecutor(); registerResult.HasError())
    {
        accepted.Close();
        return registerResult.Error();
    }

    return std::move(accepted);
}


psh::Result<NetError> Socket::Connect(const String& ip, psh::uint16 port)
{
    memset(&_sockAddr, 0, sizeof(_sockAddr));
    _sockAddr.sin_family = AF_INET;
    _sockAddr.sin_addr = Ip2Address(ip.c_str());
    _sockAddr.sin_port = htons(port);

    if (SOCKET_ERROR == connect(_socket, (SOCKADDR*)&_sockAddr, sizeof(_sockAddr)))
    {
        return NetError::FromNativeCode(WSAGetLastError());
    }

    return true;
}


IN_ADDR Socket::Ip2Address(const WCHAR* ip)
{
    IN_ADDR address{0};
    InetPtonW(AF_INET, ip, &address);
    return address;
}
