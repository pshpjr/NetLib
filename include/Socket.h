#pragma once

#include "BuildOption.h"
#include "Buffer.h"
#include "Executor.h"
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

    Socket();

    explicit Socket(Executor& executor);
    Socket(Executor& executor, SOCKET socket, SOCKADDR_IN address);

    //  등록된 상태면 Close()를 호출한다.
    ~Socket();

    //  등록 상태를 소유하므로 복사할 수 없다.
    Socket(const Socket& other) = delete;
    Socket& operator=(const Socket& other) = delete;

    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket&& other) noexcept;

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
    psh::Result<NetError> Send(SocketSendBufferSpan buffers, Executable& executable) const;
    psh::Result<NetError> Recv(SocketRecvBufferSpan buffers, Executable& executable) const;

    NetError LastError();
    psh::Result<NetError> SetLinger(bool on) const;
    psh::Result<NetError> SetNoDelay(bool on) const;
    psh::Result<NetError> SetSendBuffer(int size) const;

    SOCKADDR_IN GetSockAddress() const;
    String GetIp() const;
    psh::uint16 GetPort() const;

private:
    //  _executor에 등록한다. 성공하면 _registered = true.
    psh::Result<NetError> RegisterToExecutor();

    Executor* _executor = nullptr;
    bool _registered = false;
    NativeHandle _beforeSocket = InvalidSocket;
};
