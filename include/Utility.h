//
// Created by Park on 24. 8. 22.
//

#ifndef UTILITY_H
#define UTILITY_H
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>

#include <cstdint>

#ifndef _WIN32
#include <pthread.h>
#endif

namespace psh::util
{
    std::string WToS(std::wstring_view wstr);
    std::wstring SToW(std::string_view str);

    void set_thread_name(const char *threadName);
}

namespace psh::util
{
#ifdef _WIN32
    using TlsKey = std::uint32_t;
#else
    using TlsKey = pthread_key_t;
#endif

    bool TlsAlloc(TlsKey& outKey);
    void TlsFree(TlsKey key);
    bool TlsSet(TlsKey key, void* value);
    void* TlsGet(TlsKey key);
}

#endif //UTILITY_H


