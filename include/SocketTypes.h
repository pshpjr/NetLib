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
    ConnectionRefused,
    NetworkUnreachable,
    AddressInUse,
    AddressNotAvailable,
    PermissionDenied,
    ResourceExhausted,
    InvalidExecutor,
};

struct NetError
{
    //  라이브러리 자체 에러 코드: Executor가 Stop 이후라 소켓 등록을 거부한 경우.
#ifdef _WIN32
    //  Win32 규칙상 29번 비트가 켜진 값은 OS 에러 코드와 겹치지 않는다.
    static constexpr int InvalidExecutorCode = 0x2000'0001;
#else
    //  errno는 작은 양수(수백 이하)라 이 값과 겹치지 않는다.
    static constexpr int InvalidExecutorCode = 0x2000'0001;
#endif

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
