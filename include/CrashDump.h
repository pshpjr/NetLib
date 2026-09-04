#pragma once

#include <atomic>
#include "MyWindows.h"

class CrashDump
{
public:
    CrashDump();

    static void Crash();


    static LONG WINAPI ExceptionFilter(__in const PEXCEPTION_POINTERS pExceptionPointer);

    static void SetHandler();

    static void InvalidParamHandler(const wchar_t* expression
                                  , const wchar_t* function
                                  , const wchar_t* file
                                  , unsigned int line
                                  , uintptr_t pReserved);

    static int _custom_Report_hook(int reportType, char* message, int* returnValue);

    static void PurecallHandler();

    static std::atomic<long> _dumpCount;
};
