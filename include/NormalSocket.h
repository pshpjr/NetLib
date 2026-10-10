#pragma once
#include "SockAddr.h"
#if defined(_WIN32) || defined(_WIN64)
#include "MyWindows.h"
#endif

class NormalSocket
{
public:
    NormalSocket(const psh::NativeSocket socket, const SockAddr addr) : _sockAddr(addr)
                                                                        , _socket(socket)
    {
    }

protected:
    SockAddr _sockAddr;
    psh::NativeSocket _socket;
};
