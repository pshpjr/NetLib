#include "NormalSocket.h"

#include <WS2tcpip.h>

#include "Macro.h"
#include "Socket.h"

NetError NetError::FromNativeCode(const int nativeCode)
{
    switch (nativeCode)
    {
    case 0:
        return {NetErrc::None, nativeCode};
    case WSA_IO_PENDING:
        return {NetErrc::Pending, nativeCode};
    case WSAEINTR:
        return {NetErrc::Interrupted, nativeCode};
    case WSAECONNRESET:
    case ERROR_NETNAME_DELETED:
        return {NetErrc::ConnectionReset, nativeCode};
    case WSAECONNABORTED:
    case ERROR_CONNECTION_ABORTED:
        return {NetErrc::ConnectionAborted, nativeCode};
    case WSA_OPERATION_ABORTED:
        return {NetErrc::OperationCanceled, nativeCode};
    case WSAETIMEDOUT:
    case ERROR_SEM_TIMEOUT:
        return {NetErrc::TimedOut, nativeCode};
    case ERROR_NOT_FOUND:
        return {NetErrc::NotFound, nativeCode};
    default:
        return {NetErrc::Unknown, nativeCode};
    }
}


Socket::Socket()
    : NormalSocket(INVALID_SOCKET, {0})
{
}


Socket::Socket(const SOCKET socket, const SOCKADDR_IN address)
    : NormalSocket(socket, address)
{
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

void Socket::CancelIO() const
{
    if (const auto result = CancelIoEx(reinterpret_cast<HANDLE>(_socket), nullptr);
        result == 0)
    {
        //소켓 닫혀서 gqcs에러 받은 상황에서 send 실패하면 io 자체가 없음. 에러 아님. 
        if (const auto error = GetLastError();
            error == ERROR_NOT_FOUND)
        {
        }
        else
        {
            __debugbreak();
            printf("error : %d\n", error);
        }
    }
}

void Socket::Close()
{
    _beforeSocket = _socket;
    _socket = INVALID_SOCKET;
    closesocket(_beforeSocket);
}

psh::Result<NetError> Socket::Send(const LPWSABUF buf, const DWORD bufCount, const DWORD flag, const LPWSAOVERLAPPED lpOverlapped) const
{
    if (WSASend(_socket, buf, bufCount, nullptr, flag, lpOverlapped, nullptr) == SOCKET_ERROR)
    {
        return NetError::FromNativeCode(WSAGetLastError());
    }
    return true;
}

psh::Result<NetError> Socket::Recv(const LPWSABUF buf, const DWORD bufCount, const LPDWORD flag,
                                   const LPWSAOVERLAPPED lpOverlapped) const
{
    if (WSARecv(_socket, buf, bufCount, nullptr, flag, lpOverlapped, nullptr) == SOCKET_ERROR)
    {
        return NetError::FromNativeCode(WSAGetLastError());
    }
    return true;
}


WsaErrCode Socket::LastError()
{
    return WSAGetLastError();
}

void Socket::SetLinger(const bool on) const
{
    linger l = {on, 0};
    setsockopt(_socket, SOL_SOCKET, SO_LINGER, (char*)&l, sizeof(l));
}

void Socket::SetNoDelay(bool on) const
{
    setsockopt(_socket, IPPROTO_TCP, TCP_NODELAY, (char*)&on, sizeof(on));
}

void Socket::SetSendBuffer(const int size) const
{
    int bufSize = size;
    setsockopt(_socket, SOL_SOCKET, SO_SNDBUF, (char*)&bufSize, sizeof(bufSize));
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

bool Socket::Init()
{
    _socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (_socket == INVALID_SOCKET)
    {
        return false;
    }
    return true;
}

bool Socket::IsValid() const
{
    return _socket != INVALID_SOCKET;
}


bool Socket::Bind(const String& ip, psh::uint16 port)
{
    memset(&_sockAddr, 0, sizeof(_sockAddr));
    _sockAddr.sin_family = AF_INET;
    _sockAddr.sin_addr = Ip2Address(ip.c_str());
    _sockAddr.sin_port = htons(port);

    return SOCKET_ERROR != bind(_socket, reinterpret_cast<SOCKADDR*>(&_sockAddr), sizeof(_sockAddr));
}

bool Socket::Listen(const int backlog) const
{
    return SOCKET_ERROR != listen(_socket, backlog);
}

Socket Socket::Accept() const
{
    SOCKADDR_IN clientAddr = {};
    int len = sizeof(SOCKADDR);

    SOCKET clientSocket = accept(_socket, (SOCKADDR*)&clientAddr, &len);


    return {clientSocket, clientAddr};
}

WSAResult<bool> Socket::Connect(const String& ip, psh::uint16 port)
{
    memset(&_sockAddr, 0, sizeof(_sockAddr));
    _sockAddr.sin_family = AF_INET;
    _sockAddr.sin_addr = Ip2Address(ip.c_str());
    _sockAddr.sin_port = htons(port);

    if (SOCKET_ERROR == connect(_socket, (SOCKADDR*)&_sockAddr, sizeof(_sockAddr)))
    {
        const WsaErrCode result = WSAGetLastError();
        return result;
    }

    return true;
}


IN_ADDR Socket::Ip2Address(const WCHAR* ip)
{
    IN_ADDR address{0};
    InetPtonW(AF_INET, ip, &address);
    return address;
}
