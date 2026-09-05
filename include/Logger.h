#pragma once

#include <fmt/printf.h>

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

    template <typename... Args>
    void Write(const psh::LPCWSTR type, const LogLevel level, const psh::LPCWSTR format, Args... args)
    {
        if (static_cast<int>(_logLevel) > static_cast<int>(level))
        {
            return;
        }

        WriteInternal(type, level, fmt::sprintf(
            fmt::basic_string_view<wchar_t>(format.data(), format.size()),
            args...));
    }

private:
    void WriteInternal(std::wstring_view type, LogLevel level, std::wstring_view message);

    psh::WCHAR _dic[4][8] = {L"Debug", L"Invalid", L"Error", L"System"};

    LogLevel _logLevel;
    String _extraFileName;
};
