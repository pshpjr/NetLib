#pragma once

#include "BuildOption.h"
#include "SocketTypes.h"
#include "Types.h"
#include "Result.h"

#include SOCKET_HEADER

class Address;
class Executable;


class Socket : public SOCKET_CLASS
{
    friend class Session;

public:
    /**
     * \brief 빈 소켓 만들기
     */
    Socket();

    ~Socket() = default;

    Socket(NativeHandle socket, SOCKADDR_IN address);

    bool IsSameSubnet(const String& comp, char subnetMaskBits) const;
    static bool IsSameSubnet(const IN_ADDR& a, const IN_ADDR& b, char subnetMaskBits);

    psh::Result<NetError> Init();
    psh::Result<NetError> Bind(const String& ip, psh::uint16 port);
    psh::Result<NetError> Listen(int backlog) const;
    psh::Result<Socket, NetError> Accept() const;
    psh::Result<NetError> Connect(const String& ip, psh::uint16 port);
    psh::Result<NetError> CancelIO() const;
    psh::Result<NetError> Close();
    static IN_ADDR Ip2Address(const psh::WCHAR* ip);

    /// <summary>
    /// 
    /// </summary>
    /// <returns>이상 없으면 0, 아니면 에러코드</returns>
    bool IsValid() const;
    // Pending is an error result; retain the I/O reference until completion.
    //  executable은 추후 socket 생성자에서 executor 받으면 제거 할 예정. 그 때는 executable을 소켓이 가지게 수정한다. 
    //  지금은 executor 개념이 없어 socket 내부에 넣을 수 없다. 
    psh::Result<NetError> Send(LPWSABUF buf, DWORD bufCount, Executable& executable) const;
    psh::Result<NetError> Recv(LPWSABUF buf, DWORD bufCount, Executable& executable) const;

    NetError LastError();
    psh::Result<NetError> SetLinger(bool on) const;
    psh::Result<NetError> SetNoDelay(bool on) const;
    psh::Result<NetError> SetSendBuffer(int size) const;

    SOCKADDR_IN GetSockAddress() const;
    String GetIp() const;
    psh::uint16 GetPort() const;

private:
    NativeHandle _beforeSocket = InvalidSocket;
};
