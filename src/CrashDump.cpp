#include "CrashDump.h"

#ifdef _WIN32
#include <psapi.h>

#include <crtdbg.h>
#include <DbgHelp.h>
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>

std::atomic<long> CrashDump::_dumpCount = 0;
std::atomic<const char*> CrashDump::_buildInfo = "{\"build_version\":\"unavailable\"}\n";

void CrashDump::SetBuildInfo(const char* json)
{
    if (json != nullptr)
    {
        _buildInfo.store(json);
    }
}

void CrashDump::Crash()
{
#ifdef _WIN32
    //  접근 위반을 일으켜 ExceptionFilter에서 덤프를 남긴다.
    //  volatile이 없으면 컴파일러가 널 역참조를 제거할 수 있다.
    volatile int* p = nullptr;
    *p = 0;
#else
    //  SIGABRT로 OS가 core dump를 남긴다.
    std::abort();
#endif
}

#ifndef _WIN32
CrashDump::CrashDump()
{
}
#else
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

LONG CrashDump::ExceptionFilter(const PEXCEPTION_POINTERS pExceptionPointer)
{
    size_t iWorkingMemory = 0;
    SYSTEMTIME st;

    const long DumpCount = ++_dumpCount;

    const HANDLE hProcess = GetCurrentProcess();
    if (hProcess == nullptr)
    {
        return 0;
    }

    PROCESS_MEMORY_COUNTERS pmc;

    if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc)))
    {
        iWorkingMemory = pmc.WorkingSetSize;
    }
    CloseHandle(hProcess);

    WCHAR szFileName[MAX_PATH] = {0,};
    GetLocalTime(&st);
    swprintf(szFileName, MAX_PATH, L"DUMP_%04d%02d%02d_%02d%02d%02d_%ld_%zuMB.dmp",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, DumpCount,
             iWorkingMemory / (1024 * 1024));

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

        const char* buildInfo = _buildInfo.load();
        const auto buildInfoSize = static_cast<ULONG>(std::strlen(buildInfo));
        MINIDUMP_USER_STREAM versionStream{};
        versionStream.Type = CommentStreamA;
        versionStream.BufferSize = buildInfoSize + 1;
        versionStream.Buffer = const_cast<char*>(buildInfo);
        MINIDUMP_USER_STREAM_INFORMATION userStreams{};
        userStreams.UserStreamCount = 1;
        userStreams.UserStreamArray = &versionStream;

        const BOOL saved = MiniDumpWriteDump(GetCurrentProcess(),
                          GetCurrentProcessId(),
                          hFile,
                          MiniDumpWithFullMemory,
                          &eInfo,
                          &userStreams,
                          nullptr);
        const DWORD dumpError = saved ? ERROR_SUCCESS : GetLastError();
        CloseHandle(hFile);

        if (saved)
        {
            WCHAR metadataFileName[MAX_PATH + 16]{};
            swprintf_s(metadataFileName, L"%s.json", szFileName);
            const HANDLE metadataFile = CreateFileW(metadataFileName, GENERIC_WRITE,
                FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (metadataFile != INVALID_HANDLE_VALUE)
            {
                DWORD written = 0;
                if (!WriteFile(metadataFile, buildInfo, buildInfoSize, &written, nullptr)
                    || written != buildInfoSize)
                {
                    wprintf(L"Build version JSON write failed\n");
                }
                CloseHandle(metadataFile);
            }
            else
            {
                wprintf(L"Build version JSON creation failed: %lu\n", GetLastError());
            }
            wprintf(L"Dump File Saved\n\n\n");
        }
        else
        {
            wprintf(L"Dump File Save Failed: %lu\n", dumpError);
        }
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
    Crash();
}

int CrashDump::_custom_Report_hook(int reportType, char *message, int *returnValue)
{
    Crash();
    return true;
}

void CrashDump::PurecallHandler()
{
    Crash();
}
#endif
