#pragma once

#include "BuildOption.h"
#include "Buffer.h"
#include "Executor.h"
#include "SocketTypes.h"
#include "Types.h"
#include "Result.h"

#include SOCKET_HEADER

#include <cstddef>
#include <functional>
#include <memory>

class Address;
class Executable;


class Socket : public SOCKET_CLASS
{
    friend class Session;

public:

    Socket();

    explicit Socket(Executor& executor);
    Socket(Executor& executor, NativeHandle socket, SockAddr address);

    //  등록된 상태면 Close()를 호출한다.
    ~Socket();

    //  등록 상태를 소유하므로 복사할 수 없다.
    Socket(const Socket& other) = delete;
    Socket& operator=(const Socket& other) = delete;

    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket&& other) noexcept;

    psh::Result<NetError> Init();
    psh::Result<NetError> Bind(const String& ip, psh::uint16 port);
    psh::Result<NetError> Listen(int backlog) const;
    psh::Result<Socket, NetError> Accept() const;
    psh::Result<NetError> Connect(const String& ip, psh::uint16 port);
    psh::Result<NetError> CancelIO() const;
    psh::Result<NetError> Close();

    using IoCallback = std::function<void(NetError, std::size_t)>;

    //  이상 없으면 0, 아니면 에러코드
    bool IsValid() const;


    //  send의 경우 전달 받은 버퍼를 전부 송신 한 후 콜백 발생
    void SetCallbacks(IoCallback recvCallback, IoCallback sendCallback);
    psh::Result<NetError> Send(SocketSendBufferSpan buffers) const;
    psh::Result<NetError> Recv(SocketRecvBufferSpan buffers) const;

    NetError LastError();
    psh::Result<NetError> SetLinger(bool on) const;
    psh::Result<NetError> SetNoDelay(bool on) const;
    psh::Result<NetError> SetSendBuffer(int size) const;

    SockAddr GetSockAddress() const;
    String GetIp() const;
    psh::uint16 GetPort() const;

private:
    //  _executor에 등록한다. 성공하면 _registered = true.
    psh::Result<NetError> RegisterToExecutor();

    Executor* _executor = nullptr;
    bool _registered = false;
    NativeHandle _beforeSocket = InvalidSocket;
    std::unique_ptr<Executable> _sendExecutable;
    std::unique_ptr<Executable> _recvExecutable;
};
