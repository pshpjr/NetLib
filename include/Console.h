#pragma once

#include <fmt/xchar.h>
#include <string_view>
#include <type_traits>
#include <utility>

namespace psh::util
{
    // UTF-16 입력을 받는다. UTF-8 입력도 외부 라이브러리 메시지용으로 지원한다.
    class ConsoleStream
    {
    public:
        explicit constexpr ConsoleStream(bool error) : error_(error) {}

        ConsoleStream& operator<<(std::u16string_view text);
        ConsoleStream& operator<<(std::string_view utf8);
        ConsoleStream& operator<<(const char16_t* text);
        ConsoleStream& operator<<(const char* utf8);
        ConsoleStream& operator<<(char16_t character);
        ConsoleStream& operator<<(char character);

        template <typename T>
            requires ((std::is_arithmetic_v<T> && !std::is_same_v<T, wchar_t>) ||
                      std::is_enum_v<T>)
        ConsoleStream& operator<<(T value)
        {
            if constexpr (std::is_enum_v<T>)
                return *this << static_cast<std::underlying_type_t<T>>(value);
            else
                return *this << std::u16string_view(fmt::format(u"{}", value));
        }

    private:
        bool error_;
    };

    inline ConsoleStream cout{false};
    inline ConsoleStream cerr{true};

    template <typename... Args>
    void print(ConsoleStream& output, std::u16string_view format, Args&&... args)
    {
        output << std::u16string_view(fmt::format(format, std::forward<Args>(args)...));
    }

    template <typename... Args>
    void print(std::u16string_view format, Args&&... args)
    {
        print(cout, format, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void println(ConsoleStream& output, std::u16string_view format, Args&&... args)
    {
        auto message = fmt::format(format, std::forward<Args>(args)...);
        message.push_back(u'\n');
        output << std::u16string_view(message);
    }

    template <typename... Args>
    void println(std::u16string_view format, Args&&... args)
    {
        println(cout, format, std::forward<Args>(args)...);
    }

    inline void println(ConsoleStream& output)
    {
        output << u'\n';
    }

    inline void println()
    {
        println(cout);
    }
}
