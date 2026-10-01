#pragma once
#include "MyWindows.h"
#include "SockAddr.h"


class NormalSocket
{
public:
#if defined(_WIN32) || defined(_WIN64)
    using NativeHandle = SOCKET;
    static constexpr NativeHandle InvalidSocket = INVALID_SOCKET;
#else
    using NativeHandle = int;
    static constexpr NativeHandle InvalidSocket = -1;
#endif

    NormalSocket(const NativeHandle socket, const SockAddr addr) : _sockAddr(addr)
                                                              , _socket(socket)
    {
    }

    SockAddr _sockAddr;
    NativeHandle _socket;
};
