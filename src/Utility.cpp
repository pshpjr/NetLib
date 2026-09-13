//
// Created by Park on 24. 8. 22.
//

#include "Utility.h"

#include <iconv.h>
#include <array>
#include <cerrno>
#include <memory>
#include <system_error>
#include <cstring>


#include "concurrentqueue.h"


#ifdef _WIN32
#include <Windows.h>
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

// 디버거에게 스레드 이름을 전달하는 예외 처리
const DWORD MS_VC_EXCEPTION = 0x406D1388;
#pragma pack(push, 8)
typedef struct tagTHREADNAME_INFO {
    DWORD dwType; // 반드시 0x1000이어야 합니다.
    LPCSTR szName; // 스레드 이름
    DWORD dwThreadID; // 스레드 ID (-1로 설정하면 호출한 스레드)
    DWORD dwFlags; // 예약 필드, 0으로 설정
} THREADNAME_INFO;
#pragma pack(pop)

void psh::util::set_thread_name(const char *threadName)
{
    THREADNAME_INFO info;
    info.dwType = 0x1000;
    info.szName = threadName;
    info.dwThreadID = GetCurrentThreadId();
    info.dwFlags = 0;

    __try
    {
        RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR *) &info);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
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


