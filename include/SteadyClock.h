#pragma once

#include "Types.h"

#include <chrono>

namespace psh
{
    //  경과 시간 측정용
    class SteadyClock
    {
        using Base = std::chrono::steady_clock;

    public:
        using TimePoint = Base::time_point;
        using Duration = Base::duration;

        static TimePoint Now() { return Base::now(); }

        static int64 ToUs(const Duration duration) { return std::chrono::duration_cast<std::chrono::microseconds>(duration).count(); }
        static int64 ToMs(const Duration duration) { return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count(); }

        static int64 ElapsedUs(const TimePoint start) { return ToUs(Now() - start); }
        static int64 ElapsedMs(const TimePoint start) { return ToMs(Now() - start); }
    };

    //  시스템 시간. 로그 등
    class SystemClock
    {
        using Base = std::chrono::system_clock;

    public:
        using TimePoint = Base::time_point;
        using Duration = Base::duration;

        static TimePoint Now() { return Base::now(); }

        //  1970-01-01 UTC 기준
        static int64 ToUnixUs(const TimePoint time) { return std::chrono::duration_cast<std::chrono::microseconds>(time.time_since_epoch()).count(); }
        static int64 ToUnixMs(const TimePoint time) { return std::chrono::duration_cast<std::chrono::milliseconds>(time.time_since_epoch()).count(); }
    };
}
