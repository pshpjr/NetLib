#pragma once
#include "SockAddr.h"
#include "SocketTypes.h"

#include <span>
#include <functional>
#include <vector>

#include <sys/uio.h>    // iovec
#include <sys/socket.h> // msghdr

class LinuxSocket
{
    //  윈도우에선 WSABUF를 보존할 필요 없지만, 리눅스에선 iovec을 완료 시점까지 보존해줘야 하기에 묶음
    struct IoState
    {
        void Prepare(std::span<const iovec> source)
        {
            buffers.assign(source.begin(), source.end());
            requestedBytes = 0;
            for (const auto& buffer : buffers)
                requestedBytes += buffer.iov_len;

            message = {};
            message.msg_iov = buffers.data();
            message.msg_iovlen = buffers.size();
        }

        std::vector<iovec> buffers;
        msghdr message{};
        std::size_t requestedBytes = 0;
    };

public:
    LinuxSocket(const psh::NativeHandle socket, const SockAddr addr) : _sockAddr(addr)
                                                                        , _socket(socket)
    {
    }

    psh::NativeHandle GetNativeHandle() const
    {
        return _socket;
    }

protected:
    void OnSendComplete(NetError error, std::size_t transferred);

    SockAddr _sockAddr;
    psh::NativeHandle _socket;
    mutable IoState _sendState{};
    mutable IoState _recvState{};
    std::function<void(NetError, std::size_t)> _sendCallback;
};
