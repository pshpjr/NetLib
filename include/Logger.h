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
            return;

        //  vsprintf는 rvalue를 받지 않아, FormatArg를 호출한 결과를 바로 전달할 수 없다.
        //  아래 템플릿 WriteInternal을 활용해 포메팅한 문자열을 lvalue로 전달.

        WriteInternal(type, level, fmt::wstring_view(format.data(), format.size()), FormatArg(args)...);
    }


    template <typename... Args>
    void WriteInternal(std::wstring_view type, LogLevel level, fmt::wstring_view format, const Args&... args)
    {
        auto message = fmt::vsprintf(format, fmt::make_printf_args<wchar_t>(args...));

        WriteInternal(type, level, std::wstring_view(message));
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
