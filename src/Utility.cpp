//
// Created by Park on 24. 8. 22.
//

#include "Utility.h"

#include <codecvt>
#include <locale>

#ifdef _WIN32
#include <Windows.h>
#endif

std::string psh::util::WToS(const std::wstring &wstr)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    return converter.to_bytes(wstr);
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