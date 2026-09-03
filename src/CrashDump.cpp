#include "CrashDump.h"
#include "MemoryUsage.h"

#include <crtdbg.h>
#include <DbgHelp.h>
#include <cstdio>
#include <stdlib.h>

long CrashDump::_dumpCount = 0;

CrashDump::CrashDump()
{
    _invalid_parameter_handler oldHandler, newHandler;
    newHandler = InvalidParamHandler;

    oldHandler = _set_invalid_parameter_handler(newHandler);

    _CrtSetReportMode(_CRT_WARN, 0);
    _CrtSetReportMode(_CRT_ASSERT, 0);
    _CrtSetReportMode(_CRT_ERROR, 0);

    _CrtSetReportHook(_custom_Report_hook);

    _set_purecall_handler(PurecallHandler);

    SetHandler();
}

void CrashDump::Crash()
{
    const int* p = nullptr;
    p = nullptr;
}

LONG CrashDump::ExceptionFilter(const PEXCEPTION_POINTERS pExceptionPointer)
{
    size_t iWorkingMemory = 0;
    SYSTEMTIME st;

    const long DumpCount = InterlockedIncrement(&_dumpCount);

    MemoryUsage memoryUsage;
    memoryUsage.Update();
    iWorkingMemory = memoryUsage.GetPMemSize();

    WCHAR szFileName[MAX_PATH] = {0,};
    GetLocalTime(&st);
    wsprintfW(szFileName, L"DUMP_%04d%02d%02d_%02d%02d%02d_%d_%lldMB.dmp",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, DumpCount, iWorkingMemory);

    wprintf(L"\n\n\t\t!!!!!! CRASH DUMP !!!!!  %04d%02d%02d_%02d%02d%02d\n\n", st.wYear, st.wMonth, st.wDay
          , st.wHour, st.wMinute, st.wSecond);
    wprintf(L"Save Dump File : %s\n", szFileName);

    const HANDLE hFile = CreateFileW(
                                    szFileName,
                                    GENERIC_WRITE,
                                    FILE_SHARE_WRITE,
                                    nullptr,
                                    CREATE_ALWAYS,
                                    FILE_ATTRIBUTE_NORMAL,
                                    nullptr);

    if (hFile != INVALID_HANDLE_VALUE)
    {
        MINIDUMP_EXCEPTION_INFORMATION eInfo{};
        eInfo.ThreadId = GetCurrentThreadId();
        eInfo.ExceptionPointers = pExceptionPointer;
        eInfo.ClientPointers = TRUE;

        MiniDumpWriteDump(GetCurrentProcess(),
                          GetCurrentProcessId(),
                          hFile,
                          MiniDumpWithFullMemory,
                          &eInfo,
                          nullptr,
                          nullptr);
        CloseHandle(hFile);

        wprintf(L"Dump File Saved\n\n\n");
    }
    return EXCEPTION_EXECUTE_HANDLER;
}

void CrashDump::SetHandler()
{
    SetUnhandledExceptionFilter(ExceptionFilter);
}

void CrashDump::InvalidParamHandler(const wchar_t *expression, const wchar_t *function, const wchar_t *file
  , unsigned int line, uintptr_t pReserved)
{
    UNREFERENCED_PARAMETER(expression);
    UNREFERENCED_PARAMETER(function);
    UNREFERENCED_PARAMETER(file);
    UNREFERENCED_PARAMETER(line);
    UNREFERENCED_PARAMETER(pReserved);
    Crash();
}

int CrashDump::_custom_Report_hook(int reportType, char *message, int *returnValue)
{
    UNREFERENCED_PARAMETER(reportType);
    UNREFERENCED_PARAMETER(message);
    UNREFERENCED_PARAMETER(returnValue);
    Crash();
    return true;
}

void CrashDump::PurecallHandler()
{
    Crash();
}
