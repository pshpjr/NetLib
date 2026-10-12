#pragma once

#include "BuildOption.h"
#include "Buffer.h"
#include "Executor.h"
#include "Executable.h"
#include "SocketTypes.h"
#include "Types.h"
#include "Result.h"

#include SOCKET_HEADER

#include <cstddef>
#include <functional>
#include <memory>

class Address;


class Socket : public SOCKET_CLASS
{
    friend class Session;

public:

    Socket();

    explicit Socket(Executor& executor);
    Socket(Executor& executor, psh::NativeSocket socket, SockAddr address);

    //  등록된 상태면 Close()를 호출한다.
    ~Socket();

    //  등록 상태를 소유하므로 복사할 수 없다.
    Socket(const Socket& other) = delete;
    Socket& operator=(const Socket& other) = delete;

    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket&& other) noexcept;

    //  플랫폼별 소켓 라이브러리 초기화/정리. 소켓을 만들기 전에 InitPlatform()을 호출하고,
    //  성공했다면 모든 소켓을 닫은 뒤 CleanupPlatform()을 한 번 호출한다.
    static psh::Result<NetError> InitPlatform();
    static void CleanupPlatform();

    psh::Result<NetError> Init();
    psh::Result<NetError> Bind(Ip ip, Port port);
    psh::Result<NetError> Listen(int backlog = SOMAXCONN) const;
    void                  StopListening();
    psh::Result<Socket, NetError> Accept() const;
    psh::Result<NetError> Connect(Ip ip, Port port);
    psh::Result<NetError> CancelIO() const;
    psh::Result<NetError> Close();

    using IoCallback = std::function<void(NetError, std::size_t)>;

    //  이상 없으면 0, 아니면 에러코드
    bool IsValid() const;


    // send 성공 콜백은 전체 전송 완료 시 발생한다. 부분 완료는 PartialSend와 실제 전송량을 전달한다.
    void SetCallbacks(IoCallback recvCallback, IoCallback sendCallback);
    psh::Result<NetError> Send(SocketSendBufferSpan buffers) const;
    //  buffer 길이는 int8::max까지로 제한
    psh::Result<NetError> Recv(SocketRecvBufferSpan buffers) const;

    NetError LastError();
    psh::Result<NetError> SetLinger(bool on) const;
    psh::Result<NetError> SetNoDelay(bool on) const;
    psh::Result<NetError> SetSendBuffer(int size) const;

    SockAddr GetSockAddress() const;
    Ip GetIp() const;
    Port GetPort() const;

private:
#ifndef _WIN32
    void OnSendComplete(NetError error, std::size_t transferred);
#endif

    //  _executor에 등록한다. 성공하면 _registered = true.
    psh::Result<NetError> RegisterToExecutor();

    Executor* _executor = nullptr;
    bool _registered = false;
    psh::NativeSocket _beforeSocket = psh::InvalidSocket;
    std::unique_ptr<Executable> _sendExecutable;
    std::unique_ptr<Executable> _recvExecutable;
};
