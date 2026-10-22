#include "MetricsExporter.h"

#include <fmt/format.h>

#include <cstdio>
#include <optional>
#include <string>
#include <unordered_map>

namespace psh::metrics
{
    namespace
    {
        //  NetLib이 등록한 메트릭을 이름으로 찾아 기존 콘솔 모니터와 같은 형식으로 출력한다.
        //  없는 메트릭은 0으로 출력하고, 히스토그램은 쓰지 않는다.
        class ConsoleMonitorExporter final : public Exporter
        {
        public:
            explicit ConsoleMonitorExporter(const std::chrono::milliseconds interval)
                : _interval(interval)
            {
            }

            [[nodiscard]] std::chrono::milliseconds Interval() const override
            {
                return _interval;
            }

            void Begin(const SystemClock::TimePoint start, const SystemClock::TimePoint now) override
            {
                _seconds = std::chrono::duration<double>(now - _prevTime.value_or(start)).count();
                _prevTime = now;

                _prevValues.swap(_values);
                _values.clear();
            }

            void WriteCounter(const MetricInfo& info, const uint64 value) override
            {
                _values[Key(info)] = static_cast<double>(value);
            }

            void WriteGauge(const MetricInfo& info, const int64 value) override
            {
                _values[Key(info)] = static_cast<double>(value);
            }

            void WriteGauge(const MetricInfo& info, const double value) override
            {
                _values[Key(info)] = value;
            }

            void WriteHistogram(const MetricInfo&, std::span<const uint64>, int64) override
            {
            }

            void Flush() override
            {
                fmt::memory_buffer out;
                fmt::format_to(std::back_inserter(out),
                               "==================================================================================\n"
                               " {:<11s}{:^55s}{:>11s}\n"
                               "----------------------------------------------------------------------------------\n"
                               "+  {:<14s} : {:>6.2f}MB / {:<6.2f}MB | {:<14s} : {:>6.2f}MB / {:<6.2f}MB \n"
                               "+  {:<14s} : {:>6.2f}% / {:<6.2f}%  |\n"
                               "+  {:<14s} : {:>6.2f}% / {:<6.2f}%  |  {:<14s} : {:>6.2f}% / {:<6.2f}%  \n"
                               "----------------------------------------------------------------------------------\n"
                               "+  {:<14s} : {:>10d}\n"
                               "+  {:<14s} : {:>10d}, {:<14s} : {:>10d}\n"
                               "+  {:<14s} : {:>10d}, {:<14s} : {:>10d}\n"
                               "+  {:<14s} : {:>10d}, {:<14s} : {:>10d}\n"
                               "+  {:<14s} : {:>10d}, {:<14s} : {:>10d}\n"
                               "+  {:<14s} : {:>10d}, {:<14s} : {:>10d}\n"
                               "+  {:<14s} : {:>10d}, {:<14s} : {:>10d}\n"
                               "----------------------------------------------------------------------------------\n",
                               "+++++", "LIBS", "+++++",
                               "VirtualMem", Megabytes("process.memory.virtual"), Megabytes("process.memory.virtual.peak"),
                               "PhysicalMem", Megabytes("process.memory.physical"), Megabytes("process.memory.physical.peak"),
                               "TotalCPU", Value("process.cpu.usage{mode=total}"), Value("system.cpu.usage{mode=total}"),
                               "KernelCPU", Value("process.cpu.usage{mode=kernel}"), Value("system.cpu.usage{mode=kernel}"),
                               "UserCPU", Value("process.cpu.usage{mode=user}"), Value("system.cpu.usage{mode=user}"),
                               "Sessions", Integer("net.sessions"),
                               "Accept", Integer("net.accepts"), "AcceptTPS", PerSecond("net.accepts"),
                               "RecvTPS", PerSecond("net.packets.recv"), "SendTPS", PerSecond("net.packets.sent"),
                               "Disconnect", Integer("net.disconnects"), "DisconnectTPS", PerSecond("net.disconnects"),
                               "SegmentTimeout", Integer("net.semaphore.timeouts"), "Timeout", Integer("net.timeouts"),
                               "GSendPool", Integer("net.sendbuffer.pool.size"),
                               "GSendPoolEmpty", Integer("net.sendbuffer.pool.empty"),
                               "SendPoolAcq", Integer("net.sendbuffer.acquires"),
                               "SendPoolRel", Integer("net.sendbuffer.releases"));

                //  여러 스레드의 출력과 섞이지 않도록 한 번에 쓴다.
                std::fwrite(out.data(), 1, out.size(), stdout);
                std::fflush(stdout);
            }

        private:
            //  name 또는 name{key=value,...}. 속성은 등록할 때 키 순서로 정렬된다.
            static std::string Key(const MetricInfo& info)
            {
                std::string key = info.name;
                if (info.attributes.empty())
                    return key;

                key += '{';
                for (size_t i = 0; i < info.attributes.size(); ++i)
                    key += fmt::format("{}{}={}", i == 0 ? "" : ",", info.attributes[i].key, info.attributes[i].value);
                key += '}';
                return key;
            }

            [[nodiscard]] double Value(const std::string& key) const
            {
                const auto it = _values.find(key);
                return it != _values.end() ? it->second : 0.0;
            }

            [[nodiscard]] int64 Integer(const std::string& key) const
            {
                return static_cast<int64>(Value(key));
            }

            [[nodiscard]] double Megabytes(const std::string& key) const
            {
                return Value(key) / 1'000'000.;
            }

            //  카운터의 직전 출력 이후 초당 증가량
            [[nodiscard]] int64 PerSecond(const std::string& key) const
            {
                if (_seconds <= 0)
                    return 0;

                const auto it = _prevValues.find(key);
                const double prev = it != _prevValues.end() ? it->second : 0.0;
                return static_cast<int64>((Value(key) - prev) / _seconds + 0.5);
            }

            std::chrono::milliseconds _interval;
            std::optional<SystemClock::TimePoint> _prevTime;
            double _seconds = 0;
            std::unordered_map<std::string, double> _values;
            std::unordered_map<std::string, double> _prevValues;
        };
    }

    ExporterPtr CreateConsoleMonitorExporter(const Config& config)
    {
        return std::make_unique<ConsoleMonitorExporter>(config.consoleInterval);
    }
}
