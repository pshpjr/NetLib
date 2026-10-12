#pragma once

#include <cpptrace/from_current.hpp>

#include <cstdio>
#include <exception>
#include <functional>
#include <iostream>
#include <mutex>
#include <utility>

namespace psh::util
{
    inline void PrintCurrentExceptionTrace(const char* context) noexcept
    {
        try
        {
            static std::mutex outputMutex;
            std::lock_guard lock(outputMutex);
            std::cerr << "Exception stack [" << context << "]\n";
            cpptrace::from_current_exception().print(std::cerr);
        }
        catch (...)
        {
            std::fputs("Failed to print exception stack\n", stderr);
        }
    }

    template <typename F>
    void RunWithTrace(const char* context, F&& function)
    {
        CPPTRACE_TRY
        {
            std::invoke(std::forward<F>(function));
        }
        CPPTRACE_CATCH(...)
        {
            PrintCurrentExceptionTrace(context);
            cpptrace::rethrow();
        }
    }
}
