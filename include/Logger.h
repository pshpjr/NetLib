#pragma once

#include "Types.h"
//TODO: 멀티스레드에서 안전하게 동작하도록 수정.


class Logger
{
public:
    enum class LogLevel
    {
        Debug
        , Invalid
        , Error
        , System
    };

    Logger(psh::LPCWSTR fileName = L"", LogLevel level = LogLevel::Invalid);

    void Write(psh::LPCWSTR type, LogLevel level, psh::LPCWSTR format, ...);

private:
    psh::WCHAR _dic[4][8] = {L"Debug", L"Invalid", L"Error", L"System"};

    LogLevel _logLevel;
    String _extraFileName;
};
