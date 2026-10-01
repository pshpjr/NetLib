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

#ifdef _WIN32
#include <intrin.h>
#else
#include <csignal>
#include <pthread.h>
#endif

namespace psh::util
{
    std::string WToS(std::wstring_view wstr);
    std::wstring SToW(std::string_view str);

    void set_thread_name(const char *threadName);

    inline void DebugBreak()
    {
#ifdef _WIN32
        __debugbreak();
#else
        std::raise(SIGTRAP);
#endif
    }
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

namespace psh::util
{
    template <typename F>
    class ScopeGuard
    {
    public:
        explicit ScopeGuard(F&& fn) : fn_(std::forward<F>(fn)) {}
        ~ScopeGuard() { fn_(); }

        ScopeGuard(const ScopeGuard&) = delete;
        ScopeGuard& operator=(const ScopeGuard&) = delete;
    private:
        F fn_;
    };

    struct ScopeGuardTag
    {
    };

    template <typename F> auto operator+(ScopeGuardTag, F&& fn)
    {
        return ScopeGuard<std::decay_t<F>>(std::forward<F>(fn));
    }

}

#define SCOPE_GUARD_CONCAT_IMPL(a, b) a##b
#define SCOPE_GUARD_CONCAT(a, b) SCOPE_GUARD_CONCAT_IMPL(a, b)

#define SCOPE_GUARD const auto SCOPE_GUARD_CONCAT(scope_guard_, __COUNTER__) = psh::util::ScopeGuardTag{} + [&]()

#endif //UTILITY_H


