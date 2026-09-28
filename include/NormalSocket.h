#pragma once
#include "MyWindows.h"


class NormalSocket
{
public:
    NormalSocket(const SOCKET socket, const SOCKADDR_IN addr) : _sockAddr(addr)
                                                              , _socket(socket)
    {
    }

protected:
#if defined(_WIN32) || defined(_WIN64)
    using NativeHandle = SOCKET;
    static constexpr NativeHandle InvalidSocket = INVALID_SOCKET;
#else
    using NativeHandle = int;
    static constexpr NativeHandle InvalidSocket = -1;
#endif

    SOCKADDR_IN _sockAddr;
    SOCKET _socket;
};
