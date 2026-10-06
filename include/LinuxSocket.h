#pragma once
#include "SockAddr.h"

class LinuxSocket
{
public:
    LinuxSocket(const psh::NativeHandle socket, const SockAddr addr) : _sockAddr(addr)
                                                                        , _socket(socket)
    {
    }

protected:
    SockAddr _sockAddr;
    psh::NativeHandle _socket;
};
