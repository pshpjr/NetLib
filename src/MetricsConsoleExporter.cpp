#include "MetricsExporter.h"

#include <fmt/format.h>

#include <cstdio>
#include <optional>
#include <unordered_map>

namespace psh::metrics
{
    UnitInfo GetUnitInfo(const Unit unit)
    {
        switch (unit)
        {
        case Unit::Microseconds:
            return {"s", "us", 1e-6};
        case Unit::None:
        default:
            return {"1", "", 1.0};
        }
    }

    double EstimateQuantile(Bounds bounds, std::span<const uint64> counts, const double quantile)
    {
        uint64 total = 0;
        for (const auto count : counts)
            total += count;

        if (total == 0)
            return 0;

        const double rank = quantile * static_cast<double>(total);
        uint64 cumulative = 0;
        for (size_t i = 0; i < counts.size(); ++i)
        {
            const uint64 before = cumulative;
            cumulative += counts[i];
            if (static_cast<double>(cumulative) < rank || counts[i] == 0)
                continue;
            if (i == bounds.size())
                return static_cast<double>(bounds.back());

            const double lower = i == 0 ? 0.0 : static_cast<double>(bounds[i - 1]);
            const double upper = static_cast<double>(bounds[i]);
            return lower + (upper - lower) * (rank - static_cast<double>(before)) / static_cast<double>(counts[i]);
        }
        return static_cast<double>(bounds.back());
    }

    namespace
    {
        //  이전 출력 이후 구간의 값을 한 번에 출력한다. 카운터는 증가량과 초당 증가량, 히스토그램은 구간 백분위수를 보여준다.
        class ConsoleExporter final : public Exporter
        {
        public:
            explicit ConsoleExporter(const std::chrono::milliseconds interval)
                : _interval(interval)
            {
            }

            [[nodiscard]] std::chrono::milliseconds Interval() const override
            {
                return _interval;
            }

            void Begin(const SystemClock::TimePoint start, const SystemClock::TimePoint now) override
            {
                const auto from = _prevTime.value_or(start);
                _seconds = std::chrono::duration<double>(now - from).count();
                _prevTime = now;

                const double uptime = std::chrono::duration<double>(now - start).count();
                fmt::format_to(std::back_inserter(_out), "[metrics] uptime={:.1f}s interval={:.2f}s\n", uptime,
                               _seconds);
            }

            void WriteCounter(const MetricInfo& info, const uint64 value) override
            {
                auto& prev = _counters[&info];
                const uint64 delta = value - prev;
                prev = value;

                AppendName(info);
                fmt::format_to(std::back_inserter(_out), " +{} ({:.1f}/s) total={}{}\n", delta,
                               _seconds > 0 ? static_cast<double>(delta) / _seconds : 0.0, value,
                               GetUnitInfo(info.unit).displaySuffix);
            }

            void WriteGauge(const MetricInfo& info, const int64 value) override
            {
                AppendName(info);
                fmt::format_to(std::back_inserter(_out), " {}{}\n", value, GetUnitInfo(info.unit).displaySuffix);
            }

            void WriteHistogram(const MetricInfo& info, std::span<const uint64> counts, const int64 sum) override
            {
                auto& prev = _histograms[&info];
                prev.counts.resize(counts.size());

                std::vector<uint64> delta(counts.size());
                uint64 count = 0;
                for (size_t i = 0; i < delta.size(); ++i)
                {
                    delta[i] = counts[i] - prev.counts[i];
                    count += delta[i];
                }
                const int64 deltaSum = sum - prev.sum;
                prev.counts.assign(counts.begin(), counts.end());
                prev.sum = sum;

                AppendName(info);
                if (count == 0)
                {
                    fmt::format_to(std::back_inserter(_out), " n=0\n");
                    return;
                }

                const auto suffix = GetUnitInfo(info.unit).displaySuffix;
                fmt::format_to(std::back_inserter(_out),
                               " n={} avg={:.1f}{} p50={:.1f}{} p95={:.1f}{} p99={:.1f}{}{}\n", count,
                               static_cast<double>(deltaSum) / static_cast<double>(count), suffix,
                               EstimateQuantile(info.bounds, delta, 0.50), suffix,
                               EstimateQuantile(info.bounds, delta, 0.95), suffix,
                               EstimateQuantile(info.bounds, delta, 0.99), suffix,
                               delta.back() > 0 ? fmt::format(" over_last_bound={}", delta.back()) : "");
            }

            //  여러 스레드의 출력과 섞이지 않도록 한 번에 쓴다.
            void Flush() override
            {
                std::fwrite(_out.data(), 1, _out.size(), stdout);
                std::fflush(stdout);
                _out.clear();
            }

        private:
            void AppendName(const MetricInfo& info)
            {
                fmt::format_to(std::back_inserter(_out), "  {}", info.name);
                if (info.attributes.empty())
                    return;

                _out.push_back('{');
                for (size_t i = 0; i < info.attributes.size(); ++i)
                {
                    fmt::format_to(std::back_inserter(_out), "{}{}={}", i == 0 ? "" : ",", info.attributes[i].key,
                                   info.attributes[i].value);
                }
                _out.push_back('}');
            }

            struct HistogramState
            {
                std::vector<uint64> counts;
                int64 sum = 0;
            };

            std::chrono::milliseconds _interval;
            fmt::memory_buffer _out;
            std::optional<SystemClock::TimePoint> _prevTime;
            double _seconds = 0;
            //  저장소마다 MetricInfo가 하나라 주소로 구분한다.
            std::unordered_map<const MetricInfo*, uint64> _counters;
            std::unordered_map<const MetricInfo*, HistogramState> _histograms;
        };
    }

    ExporterPtr CreateConsoleExporter(const Config& config)
    {
        return std::make_unique<ConsoleExporter>(config.consoleInterval);
    }
}
