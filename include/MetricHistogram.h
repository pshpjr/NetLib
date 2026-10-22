#pragma once

#include "SteadyClock.h"
#include "MetricHandles.h"

#include <span>

//  히스토그램도 핸들인데, 연관 타입이 많아서 따로 분리함. 
namespace psh::metrics
{
    //  히스토그램 버킷 경계. 오름차순, 상한 포함. 마지막 경계 뒤에 +Inf 버킷이 붙는다.
    using Bounds = std::span<const int64>;

    class Histogram
    {
        friend class Registry;

    public:
        struct Storage;

        //  RecordSince에 넘기는 시작 시각. 메트릭이 꺼지면 빈 타입이 된다.
        //  멤버로 둘 때는 METRIC_TIME_POINT(이름);으로 선언해야 꺼졌을 때 크기가 늘지 않는다.
        //  지역 변수는 그냥 선언해도 된다.
#ifdef NETLIB_ENABLE_METRICS
        class TimePoint
        {
        public:
            TimePoint() = default;

            static TimePoint Now() { return SteadyClock::Now(); }

            [[nodiscard]] int64 ElapsedUs() const { return SteadyClock::ElapsedUs(_time); }

        private:
            TimePoint(SteadyClock::TimePoint time);

            SteadyClock::TimePoint _time;
        };
#else
        class TimePoint
        {
        public:
            static TimePoint Now() { return {}; }

            [[nodiscard]] int64 ElapsedUs() const
            {
                return 0;
            }
        };
#endif

        //  지연 시간용 기본 경계. 1us ~ 1s
        static Bounds LatencyUsBounds();

        Histogram() = default;

        void Record(int64 value) const { if constexpr (Enabled) { RecordImpl(value); } }

        //  경과 시간을 마이크로초로 기록한다. CreateTimeHistogram으로 등록한 히스토그램에 쓴다.
        void RecordSince(const TimePoint& start) const { if constexpr (Enabled) { RecordImpl(start.ElapsedUs()); } }

        [[nodiscard]] const MetricInfo* Info() const;

    private:
        explicit Histogram(Storage* storage);

        void RecordImpl(int64 value) const;

        Storage* _storage = nullptr;
    };
}

//  TimePoint 멤버 선언. 메트릭이 꺼지면 크기를 차지하지 않는다. 예: METRIC_TIME_POINT(enqueuedAt);
#if defined(_MSC_VER)
#define METRIC_TIME_POINT(name) [[msvc::no_unique_address]] psh::metrics::Histogram::TimePoint name
#else
#define METRIC_TIME_POINT(name) [[no_unique_address]] psh::metrics::Histogram::TimePoint name
#endif
