#ifdef _WIN32
#include "MyWindows.h"
#else
#include <unistd.h>
#endif

#include "Console.h"
#include "Utility.h"

#include <algorithm>
#include <cerrno>
#include <mutex>
#include <stdexcept>
#include <system_error>

namespace
{
    std::mutex outputMutex;

#ifdef _WIN32
    HANDLE OutputHandle(bool error)
    {
        const auto handle = GetStdHandle(error ? STD_ERROR_HANDLE : STD_OUTPUT_HANDLE);
        if (handle == INVALID_HANDLE_VALUE)
            throw std::system_error(static_cast<int>(GetLastError()),
                                    std::system_category(), "GetStdHandle");
        if (handle == nullptr)
            throw std::system_error(ERROR_INVALID_HANDLE, std::system_category(),
                                    "Standard output handle is not available");
        return handle;
    }

    bool IsConsole(HANDLE handle)
    {
        DWORD mode = 0;
        if (GetConsoleMode(handle, &mode))
            return true;

        // GetConsoleMode 실패가 잘못된 핸들 때문인지 구분한다.
        SetLastError(ERROR_SUCCESS);
        if (GetFileType(handle) == FILE_TYPE_UNKNOWN)
        {
            const auto error = GetLastError();
            if (error != ERROR_SUCCESS)
                throw std::system_error(static_cast<int>(error),
                                        std::system_category(), "GetFileType");
        }
        return false;
    }

    void WriteConsoleText(HANDLE handle, std::u16string_view text)
    {
        while (!text.empty())
        {
            auto count = (std::min)(text.size(), std::size_t{32768});
            if (count < text.size() && text[count - 1] >= 0xD800 && text[count - 1] <= 0xDBFF)
                --count;
            DWORD written = 0;
            // WriteConsoleW의 입력은 const void*이므로 wchar_t 복사/변환이 필요 없다.
            if (!WriteConsoleW(handle, text.data(), static_cast<DWORD>(count), &written, nullptr))
                throw std::system_error(static_cast<int>(GetLastError()),
                                        std::system_category(), "WriteConsoleW");
            if (written == 0)
                throw std::runtime_error("WriteConsoleW made no progress");
            text.remove_prefix(written);
        }
    }

    void WriteBytes(HANDLE handle, std::string_view bytes)
    {
        while (!bytes.empty())
        {
            const auto count = static_cast<DWORD>((std::min)(bytes.size(), std::size_t{32768}));
            DWORD written = 0;
            if (!WriteFile(handle, bytes.data(), count, &written, nullptr))
                throw std::system_error(static_cast<int>(GetLastError()),
                                        std::system_category(), "WriteFile");
            if (written == 0)
                throw std::runtime_error("WriteFile made no progress");
            bytes.remove_prefix(written);
        }
    }
#else
    void WriteBytes(bool error, std::string_view bytes)
    {
        while (!bytes.empty())
        {
            const auto count = (std::min)(bytes.size(), std::size_t{32768});
            const auto written = ::write(error ? STDERR_FILENO : STDOUT_FILENO, bytes.data(), count);
            if (written < 0)
            {
                if (errno == EINTR)
                    continue;
                throw std::system_error(errno, std::generic_category(), "write");
            }
            if (written == 0)
                throw std::runtime_error("write made no progress");
            bytes.remove_prefix(static_cast<std::size_t>(written));
        }
    }
#endif
}

psh::util::ConsoleStream& psh::util::ConsoleStream::operator<<(std::u16string_view text)
{
    if (text.empty())
        return *this;
    std::lock_guard lock(outputMutex);
#ifdef _WIN32
    const auto handle = OutputHandle(error_);
    if (IsConsole(handle))
        WriteConsoleText(handle, text);
    else
        WriteBytes(handle, Utf16ToUtf8(text));
#else
    WriteBytes(error_, Utf16ToUtf8(text));
#endif
    return *this;
}

psh::util::ConsoleStream& psh::util::ConsoleStream::operator<<(std::string_view utf8)
{
    if (utf8.empty())
        return *this;
    std::lock_guard lock(outputMutex);
#ifdef _WIN32
    const auto handle = OutputHandle(error_);
    if (IsConsole(handle))
        WriteConsoleText(handle, Utf8ToUtf16(utf8));
    else
        WriteBytes(handle, utf8);
#else
    WriteBytes(error_, utf8);
#endif
    return *this;
}

psh::util::ConsoleStream& psh::util::ConsoleStream::operator<<(const char16_t* text)
{
    if (text == nullptr)
        throw std::invalid_argument("Null UTF-16 output string");
    return *this << std::u16string_view(text);
}

psh::util::ConsoleStream& psh::util::ConsoleStream::operator<<(const char* utf8)
{
    if (utf8 == nullptr)
        throw std::invalid_argument("Null UTF-8 output string");
    return *this << std::string_view(utf8);
}

psh::util::ConsoleStream& psh::util::ConsoleStream::operator<<(char16_t character)
{
    return *this << std::u16string_view(&character, 1);
}

psh::util::ConsoleStream& psh::util::ConsoleStream::operator<<(char character)
{
    return *this << std::string_view(&character, 1);
}
