#pragma once

#include "Types.h"

#include <chrono>

namespace psh
{
    //  경과 시간 측정용 시계. 다른 시계로 바꿀 때는 이 파일만 고친다.
    //  벽시계 시각(로그, 내보내기 타임스탬프)이 필요하면 system_clock을 쓴다.
    class Time
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
}
