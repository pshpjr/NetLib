#pragma once

#include "Types.h"

#include <fmt/printf.h>
#include <fmt/xchar.h>

#include <thread>
#include <type_traits>
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

    Logger(std::wstring_view fileName = L"", LogLevel level = LogLevel::Invalid);

    template <typename... Args>
    void Write(const std::wstring_view type, const LogLevel level, const std::wstring_view format, const Args&... args)
    {
        if (static_cast<int>(_logLevel) > static_cast<int>(level))
        {
            return;
        }

        WriteInternal(type, level, fmt::sprintf(
            fmt::basic_string_view<wchar_t>(format.data(), format.size()),
            FormatArg(args)...));
    }

private:
    template <typename T>
    static auto FormatArg(const T& value)
    {
        if constexpr (requires { value.load(); }) return FormatArg(value.load());
        else if constexpr (std::is_convertible_v<T, std::string_view>) return psh::util::SToW(std::string_view(value));
        else if constexpr (requires { typename T::period; value.count(); }) return value.count();
        else if constexpr (std::is_same_v<T, std::thread::id>) return std::hash<std::thread::id>{}(value);
        else if constexpr (std::is_enum_v<T>) return static_cast<std::underlying_type_t<T>>(value);
        else if constexpr (requires { value.id; }) return value.id;
        else if constexpr (std::is_class_v<T> && requires { static_cast<long>(value); }) return static_cast<long>(value);
        else return value;
    }

    void WriteInternal(std::wstring_view type, LogLevel level, std::wstring_view message);

    psh::WCHAR _dic[4][8] = {L"Debug", L"Invalid", L"Error", L"System"};

    LogLevel _logLevel;
    String _extraFileName;
};
