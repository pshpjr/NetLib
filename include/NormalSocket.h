#pragma once
#include "MyWindows.h"


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

    NormalSocket(const NativeHandle socket, const SOCKADDR_IN addr) : _sockAddr(addr)
                                                              , _socket(socket)
    {
    }

    SOCKADDR_IN _sockAddr;
    NativeHandle _socket;
};
