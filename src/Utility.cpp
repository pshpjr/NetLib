//
// Created by Park on 24. 8. 22.
//

#include "Utility.h"

#include <iconv.h>
#include <algorithm>
#include <array>
#include <cerrno>
#include <memory>
#include <system_error>
#include <cstring>
#include <bit>

#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE 1
#endif

#if defined(_WIN32) || defined(_WIN64)
#include <Windows.h>
#else
#include <sys/syscall.h>
#include <unistd.h>
#include <pthread.h>
#endif

namespace
{
std::string ConvertEncoding(std::string_view input, const char* to, const char* from)
{
    if (input.empty()) return {};
    const auto descriptor = iconv_open(to, from);
    if (descriptor == reinterpret_cast<iconv_t>(-1))
        throw std::system_error(errno, std::generic_category(), "iconv_open");
    const std::unique_ptr<void, decltype(&iconv_close)> converter(descriptor, &iconv_close);

    std::string source(input);
    char* sourcePtr = source.data();
    size_t sourceBytes = source.size();
    std::array<char, 1024> buffer;
    std::string output;
    const auto convert = [&](bool flush)
    {
        for (;;)
        {
            char* destination = buffer.data();
            size_t available = buffer.size();
            const auto result = iconv(converter.get(), flush ? nullptr : &sourcePtr,
                flush ? nullptr : &sourceBytes, &destination, &available);
            const int error = errno;
            output.append(buffer.data(), buffer.size() - available);
            if (result != static_cast<size_t>(-1)) return;
            if (error != E2BIG)
                throw std::system_error(error, std::generic_category(), "iconv");
        }
    };
    convert(false);
    convert(true);
    return output;
}
}

std::string psh::util::WToS(std::wstring_view wstr)
{
    if (wstr.empty()) return {};
    return ConvertEncoding({reinterpret_cast<const char*>(wstr.data()),
        wstr.size() * sizeof(wchar_t)}, "UTF-8", "WCHAR_T");
}

std::wstring psh::util::SToW(std::string_view str)
{
    if (str.empty()) return {};
    const auto bytes = ConvertEncoding(str, "WCHAR_T", "UTF-8");

    std::wstring result(bytes.size() / sizeof(wchar_t), L'\0');
    std::memcpy(result.data(), bytes.data(), bytes.size());
    return result;
}

std::string psh::util::Utf16ToUtf8(std::u16string_view text)
{
    static_assert(sizeof(char16_t) == 2);
    static_assert(std::endian::native == std::endian::little ||
                  std::endian::native == std::endian::big);
    const char* encoding = std::endian::native == std::endian::little ? "UTF-16LE" : "UTF-16BE";
    return ConvertEncoding({reinterpret_cast<const char*>(text.data()), text.size() * 2},
                           "UTF-8", encoding);
}

std::u16string psh::util::Utf8ToUtf16(std::string_view text)
{
    const char* encoding = std::endian::native == std::endian::little ? "UTF-16LE" : "UTF-16BE";
    const auto bytes = ConvertEncoding(text, encoding, "UTF-8");
    std::u16string result(bytes.size() / 2, u'\0');
    if (!bytes.empty())
        std::memcpy(result.data(), bytes.data(), bytes.size());
    return result;
}

std::uint32_t psh::util::GetCurrentThreadId() noexcept
{
#if defined(_WIN32) || defined(_WIN64)
    return static_cast<std::uint32_t>(::GetCurrentThreadId());
#else
    return static_cast<std::uint32_t>(::syscall(SYS_gettid));
#endif
}

void psh::util::SetThreadName(const char* threadName)
{
    if (threadName == nullptr) return;
#ifdef _WIN32
    const auto name = SToW(threadName);

    SetThreadDescription(::GetCurrentThread(), name.c_str());
#else
    // Linux includes the terminating null in its 16-byte name limit.
    std::array<char, 16> name{};
    
    const auto length = std::min(std::strlen(threadName), name.size() - 1);
    
    std::memcpy(name.data(), threadName, length);
    
    pthread_setname_np(::pthread_self(), name.data());
#endif
}

bool psh::util::TlsAlloc(TlsKey& outKey)
{
#ifdef _WIN32
    outKey = ::TlsAlloc();
    return outKey != TLS_OUT_OF_INDEXES;
#else
    return ::pthread_key_create(&outKey, nullptr) == 0;
#endif
}

void psh::util::TlsFree(TlsKey key)
{
#ifdef _WIN32
    ::TlsFree(key);
#else
    ::pthread_key_delete(key);
#endif
}

bool psh::util::TlsSet(TlsKey key, void* value)
{
#ifdef _WIN32
    return ::TlsSetValue(key, value) != FALSE;
#else
    return ::pthread_setspecific(key, value) == 0;
#endif
}

void* psh::util::TlsGet(TlsKey key)
{
#ifdef _WIN32
    return ::TlsGetValue(key);
#else
    return ::pthread_getspecific(key);
#endif
}


