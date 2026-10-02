#pragma once

#ifdef _WIN32
#include "MyWindows.h"
#endif

#include <atomic>
#include <cstdint>

//  리눅스에선 OS가 남김
class CrashDump
{
public:
    CrashDump();

    static void Crash();

    // 워커 실행 전 등록, 인자는 static 리터럴이여야 함
    static void SetBuildInfo(const char* json);

#ifdef _WIN32
    static LONG WINAPI ExceptionFilter(__in const PEXCEPTION_POINTERS pExceptionPointer);

    static void SetHandler();

    static void InvalidParamHandler(const wchar_t* expression
                                  , const wchar_t* function
                                  , const wchar_t* file
                                  , unsigned int line
                                  , uintptr_t pReserved);

    static int _custom_Report_hook(int reportType, char* message, int* returnValue);

    static void PurecallHandler();
#endif

    static std::atomic<long> _dumpCount;

private:
    static std::atomic<const char*> _buildInfo;
};
