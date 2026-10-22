#include "MetricsExporter.h"

#include <fmt/base.h>

#include <opentelemetry/exporters/otlp/otlp_http_metric_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_http_metric_exporter_options.h>
#include <opentelemetry/sdk/common/exporter_utils.h>
#include <opentelemetry/sdk/common/global_log_handler.h>
#include <opentelemetry/sdk/instrumentationscope/instrumentation_scope.h>
#include <opentelemetry/sdk/metrics/data/metric_data.h>
#include <opentelemetry/sdk/metrics/data/point_data.h>
#include <opentelemetry/sdk/metrics/export/metric_producer.h>
#include <opentelemetry/sdk/metrics/push_metric_exporter.h>
#include <opentelemetry/sdk/resource/resource.h>

#include <map>
#include <mutex>
#include <unordered_map>

/*
    SDK의 MeterProvider·계측기를 사용하지 않는다. 동기 계측기는 기록마다 속성 해시와 저장소 잠금을 거치기 때문이다.
    Registry가 집계한 누적값을 SDK 데이터 구조로 옮겨 OTLP/HTTP exporter에 직접 전달한다.
    otel-cpp를 올릴 때 이 파일만 확인하면 되도록 SDK 타입은 이 파일 밖으로 노출하지 않는다.
 */
namespace psh::metrics
{
    namespace
    {
        namespace otlp = opentelemetry::exporter::otlp;
        namespace sdk = opentelemetry::sdk;
        namespace sdkmetrics = opentelemetry::sdk::metrics;

        namespace internal_log = opentelemetry::sdk::common::internal_log;

        /// <summary>
        /// 같은 위치의 OTel 내부 로그는 주기마다 한 번만 출력한다.
        /// 수집기가 꺼져 있으면 내보낼 때마다 연결 실패 로그가 찍히기 때문이다. 생략한 횟수는 다음 출력에 붙인다.
        /// </summary>
        class RateLimitedLogHandler final : public internal_log::LogHandler
        {
        public:
            void Handle(const internal_log::LogLevel level, const char* file, const int line, const char* msg,
                        const sdk::common::AttributeMap&) noexcept override
            {
                try
                {
                    const std::string_view path = file != nullptr ? file : "";
                    const auto now = SteadyClock::Now();
                    uint64 suppressed = 0;
                    {
                        std::lock_guard guard(_lock);
                        auto& entry = _entries[{path, line}];
                        if (entry.printed && now - entry.lastPrint < Interval)
                        {
                            ++entry.suppressed;
                            return;
                        }
                        entry.printed = true;
                        entry.lastPrint = now;
                        suppressed = std::exchange(entry.suppressed, 0);
                    }

                    const auto name = path.substr(path.find_last_of("/\\") + 1);
                    if (suppressed == 0)
                    {
                        fmt::print(stderr, "[otel] [{}] {} ({}:{})\n", internal_log::LevelToString(level),
                                   msg != nullptr ? msg : "", name, line);
                    }
                    else
                    {
                        fmt::print(stderr, "[otel] [{}] {} ({}:{}, {} repeated messages suppressed)\n",
                                   internal_log::LevelToString(level), msg != nullptr ? msg : "", name, line,
                                   suppressed);
                    }
                }
                catch (...)
                {
                }
            }

        private:
            static constexpr auto Interval = std::chrono::seconds(60);

            struct Entry
            {
                bool printed = false;
                SteadyClock::TimePoint lastPrint;
                uint64 suppressed = 0;
            };

            std::mutex _lock;
            //  file은 OTel 소스의 __FILE__ 문자열 상수이므로 포인터를 보관해도 된다.
            std::map<std::pair<std::string_view, int>, Entry> _entries;
        };

        //  사용자가 직접 설정한 핸들러는 건드리지 않는다. 기본 핸들러일 때만 교체한다.
        void InstallLogHandler()
        {
            const auto current = internal_log::GlobalLogHandler::GetLogHandler();
            if (dynamic_cast<internal_log::DefaultLogHandler*>(current.get()) == nullptr)
                return;
            internal_log::GlobalLogHandler::SetLogHandler(
                opentelemetry::nostd::shared_ptr<internal_log::LogHandler>(new RateLimitedLogHandler));
        }

        sdkmetrics::PointAttributes ToPointAttributes(const Attributes& attributes)
        {
            sdkmetrics::PointAttributes result;
            for (const auto& [key, value] : attributes)
            {
                result.SetAttribute(key, value);
            }
            return result;
        }

        class OtlpExporter final : public Exporter
        {
        public:
            OtlpExporter(const Config& config, std::unique_ptr<sdkmetrics::PushMetricExporter> exporter)
                : _interval(config.otlpInterval)
                , _endpoint(config.otlpEndpoint)
                , _exporter(std::move(exporter))
                , _scope(sdk::instrumentationscope::InstrumentationScope::Create("psh.netlib"))
            {
                sdk::resource::ResourceAttributes attributes;
                for (const auto& [key, value] : config.resourceAttributes)
                {
                    attributes.SetAttribute(key, value);
                }

                const std::pair<const char*, const std::string*> known[] = {
                    {"service.name", &config.serviceName},
                    {"service.version", &config.serviceVersion},
                    {"service.instance.id", &config.serviceInstanceId},
                    {"deployment.environment.name", &config.environment},
                };
                for (const auto& [key, value] : known)
                {
                    if (!value->empty())
                    {
                        attributes.SetAttribute(key, *value);
                    }
                }

                _resource = std::make_unique<sdk::resource::Resource>(sdk::resource::Resource::Create(attributes));
            }

            [[nodiscard]] std::chrono::milliseconds Interval() const override
            {
                return _interval;
            }

            void Begin(const SystemClock::TimePoint start, const SystemClock::TimePoint now) override
            {
                _start = opentelemetry::common::SystemTimestamp(start);
                _now = opentelemetry::common::SystemTimestamp(now);
            }

            void WriteCounter(const MetricInfo& info, const uint64 value) override
            {
                const double scale = GetUnitInfo(info.unit).toExported;
                sdkmetrics::SumPointData sum;
                sum.is_monotonic_ = true;
                if (scale == 1.0)
                    sum.value_ = static_cast<int64_t>(value);
                else
                    sum.value_ = static_cast<double>(value) * scale;

                AddPoint(info, sdkmetrics::InstrumentType::kCounter, ValueType(scale), sum);
            }

            void WriteGauge(const MetricInfo& info, const int64 value) override
            {
                const double scale = GetUnitInfo(info.unit).toExported;
                sdkmetrics::LastValuePointData gauge;
                gauge.is_lastvalue_valid_ = true;
                gauge.sample_ts_ = _now;
                if (scale == 1.0)
                    gauge.value_ = static_cast<int64_t>(value);
                else
                    gauge.value_ = static_cast<double>(value) * scale;

                AddPoint(info, sdkmetrics::InstrumentType::kObservableGauge, ValueType(scale), gauge);
            }

            void WriteGauge(const MetricInfo& info, const double value) override
            {
                sdkmetrics::LastValuePointData gauge;
                gauge.is_lastvalue_valid_ = true;
                gauge.sample_ts_ = _now;
                gauge.value_ = value * GetUnitInfo(info.unit).toExported;

                AddPoint(info, sdkmetrics::InstrumentType::kObservableGauge, sdkmetrics::InstrumentValueType::kDouble,
                         gauge);
            }

            void WriteHistogram(const MetricInfo& info, std::span<const uint64> counts, const int64 sum) override
            {
                const double scale = GetUnitInfo(info.unit).toExported;
                sdkmetrics::HistogramPointData histogram;
                histogram.boundaries_.reserve(info.bounds.size());
                for (const auto bound : info.bounds)
                    histogram.boundaries_.push_back(static_cast<double>(bound) * scale);

                histogram.counts_.assign(counts.begin(), counts.end());
                for (const auto count : counts)
                    histogram.count_ += count;

                histogram.sum_ = static_cast<double>(sum) * scale;
                histogram.record_min_max_ = false;

                AddPoint(info, sdkmetrics::InstrumentType::kHistogram, sdkmetrics::InstrumentValueType::kDouble,
                         histogram);
            }

            void Flush() override
            {
                sdkmetrics::ScopeMetrics scopeMetrics;
                scopeMetrics.scope_ = _scope.get();
                scopeMetrics.metric_data_ = std::move(_metrics);
                _metrics.clear();
                _indexByName.clear();

                if (scopeMetrics.metric_data_.empty())
                    return;

                sdkmetrics::ResourceMetrics resourceMetrics;
                resourceMetrics.resource_ = _resource.get();
                resourceMetrics.scope_metric_data_.push_back(std::move(scopeMetrics));

                ReportResult(_exporter->Export(resourceMetrics));
            }

            void Shutdown() override
            {
                _exporter->Shutdown();
            }

        private:
            static sdkmetrics::InstrumentValueType ValueType(const double scale)
            {
                return scale == 1.0 ? sdkmetrics::InstrumentValueType::kLong : sdkmetrics::InstrumentValueType::kDouble;
            }

            //  OTLP에서는 이름 하나가 계측기 하나이고, 속성 조합마다 포인트를 하나씩 가진다.
            void AddPoint(const MetricInfo& info, const sdkmetrics::InstrumentType type,
                          const sdkmetrics::InstrumentValueType valueType, sdkmetrics::PointType point)
            {
                const auto [found, inserted] = _indexByName.try_emplace(info.name, _metrics.size());
                if (inserted)
                {
                    sdkmetrics::MetricData data;
                    data.instrument_descriptor = {info.name, info.description,
                                                  std::string(GetUnitInfo(info.unit).otelUnit), type, valueType};
                    data.aggregation_temporality = sdkmetrics::AggregationTemporality::kCumulative;
                    data.start_ts = _start;
                    data.end_ts = _now;
                    _metrics.push_back(std::move(data));
                }
                _metrics[found->second].point_data_attr_.push_back({ToPointAttributes(info.attributes), point});
            }

            //  수집기가 꺼져 있어도 로그가 넘치지 않도록 상태가 바뀔 때만 출력한다.
            void ReportResult(const sdk::common::ExportResult result)
            {
                const bool success = result == sdk::common::ExportResult::kSuccess;
                if (success == _lastSuccess)
                    return;
                _lastSuccess = success;

                if (success)
                {
                    fmt::print(stderr, "[metrics] otlp export recovered: {}\n", _endpoint);
                }
                else
                {
                    fmt::print(stderr, "[metrics] otlp export failed: {} (result={})\n", _endpoint,
                               static_cast<int>(result));
                }
            }

            std::chrono::milliseconds _interval;
            std::string _endpoint;
            std::unique_ptr<sdkmetrics::PushMetricExporter> _exporter;
            std::unique_ptr<sdk::instrumentationscope::InstrumentationScope> _scope;
            std::unique_ptr<sdk::resource::Resource> _resource;
            bool _lastSuccess = true;

            //  Begin부터 Flush까지 모은 계측기. _indexByName의 키는 MetricInfo::name을 가리킨다.
            opentelemetry::common::SystemTimestamp _start;
            opentelemetry::common::SystemTimestamp _now;
            std::vector<sdkmetrics::MetricData> _metrics;
            std::unordered_map<std::string_view, size_t> _indexByName;
        };
    }

    ExporterPtr CreateOtlpExporter(const Config& config)
    {
        InstallLogHandler();

        otlp::OtlpHttpMetricExporterOptions options;
        options.url = config.otlpEndpoint;
        options.timeout = std::chrono::duration_cast<std::chrono::system_clock::duration>(config.otlpTimeout);
        options.aggregation_temporality = otlp::PreferredAggregationTemporality::kCumulative;

        return std::make_unique<OtlpExporter>(config, otlp::OtlpHttpMetricExporterFactory::Create(options));
    }
}
