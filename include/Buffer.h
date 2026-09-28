#pragma once

#include <cassert>
#include <cstddef>
#include <limits>
#include <span>

#if defined(_WIN32)
#include "MyWindows.h"
using SocketBuffer = WSABUF;
#else
#include <sys/uio.h>
using SocketBuffer = iovec;
#endif

//  os별로 다른 소켓 버퍼를 쓰기 위해
using SocketSendBuffer = SocketBuffer;
using SocketRecvBuffer = SocketBuffer;
using SocketSendBufferSpan = std::span<SocketSendBuffer>;
using SocketRecvBufferSpan = std::span<SocketRecvBuffer>;

inline void SetSocketBuffer(SocketBuffer& buffer, char* buf, std::size_t len)
{
#if defined(_WIN32)
    assert(len <= (std::numeric_limits<ULONG>::max)());
    buffer.buf = buf;
    buffer.len = static_cast<ULONG>(len);
#else
    buffer.iov_base = buf;
    buffer.iov_len = len;
#endif
}
