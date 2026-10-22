#pragma once

#include "Metrics.h"

#include <chrono>
#include <memory>
#include <span>
#include <string_view>

//  NetLib 내부 전용.
namespace psh::metrics
{
    //  내보내기 스레드가 주기마다 등록 잠금을 잡고 Begin, 저장소마다 Write~를 부른 뒤, 잠금을 풀고 Flush를 부른다.
    //  값은 모두 시작 이후 누적값이다.
    class Exporter
    {
    public:
        virtual ~Exporter() = default;

        [[nodiscard]] virtual std::chrono::milliseconds Interval() const = 0;

        virtual void Begin(SystemClock::TimePoint start, SystemClock::TimePoint now) = 0;
        virtual void WriteCounter(const MetricInfo& info, uint64 value) = 0;
        virtual void WriteGauge(const MetricInfo& info, int64 value) = 0;
        //  counts는 info.bounds.size() + 1개. 마지막은 +Inf 버킷이다.
        virtual void WriteHistogram(const MetricInfo& info, std::span<const uint64> counts, int64 sum) = 0;
        virtual void Flush() = 0;

        virtual void Shutdown()
        {
        }
    };

    using ExporterPtr = std::unique_ptr<Exporter>;

    ExporterPtr CreateConsoleExporter(const Config& config);

#ifdef NETLIB_ENABLE_OTEL
    //  OTel 타입은 MetricsOtlpExporter.cpp 밖으로 나오지 않는다.
    ExporterPtr CreateOtlpExporter(const Config& config);
#endif

    //  단위별 내보내기 이름과 배율. 시간 단위는 초로 변환한다.
    struct UnitInfo
    {
        std::string_view otelUnit;
        std::string_view displaySuffix;
        double toExported;
    };

    UnitInfo GetUnitInfo(Unit unit);

    //  Prometheus histogram_quantile과 같은 버킷 내 선형 보간. 마지막 버킷이면 마지막 경계를 반환한다.
    double EstimateQuantile(Bounds bounds, std::span<const uint64> counts, double quantile);
}
