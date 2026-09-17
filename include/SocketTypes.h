//
// Created by pshpj on 24. 9. 2.
//

#ifndef SOCKETTYPES_H
#define SOCKETTYPES_H
#include "MyWindows.h"
#include "Types.h"

//소켓 관련
using Port = psh::uint16;
using SockAddr_in = SOCKADDR_IN;
using SocketID = psh::uint64;

enum class NetErrc
{
    None,
    Pending,
    Interrupted,
    ConnectionReset,
    ConnectionAborted,
    OperationCanceled,
    TimedOut,
    NotFound,
    Unknown,
};

struct NetError
{
    int NativeCode = 0;

    operator NetErrc() const
    {
        return ToNetErrc(NativeCode);
    }

    int GetNativeCode() const noexcept
    {
        return NativeCode;
    }

    static NetError FromNativeCode(int nativeCode);
    static NetErrc  ToNetErrc(int nativeCode);
};
#endif //SOCKETTYPES_H
