#include "MetricRegistry.h"

#include "MetricStorage.h"

#include <fmt/format.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <mutex>
#include <semaphore>
#include <thread>

namespace psh::metrics
{
    /////////////////////
    //      공유 상태
    /////////////////////

    namespace
    {
        std::mutex& StoragesLock()
        {
            static auto* lock = new std::mutex;
            return *lock;
        }

        //  StoragesLock()을 잡고 접근한다.
        std::vector<std::unique_ptr<StorageBase>>& GetStorages()
        {
            static auto* storages = new std::vector<std::unique_ptr<StorageBase>>;
            return *storages;
        }

        //  측정 시작 시간
        const auto gStart = SystemClock::Now();
    }

    /////////////////////
    //      Registry
    /////////////////////

    namespace
    {
        void LogError(std::string_view name, std::string_view reason)
        {
            fmt::print(stderr, "[metrics] rejected '{}': {}\n", name, reason);
        }

        bool IsValidName(std::string_view name)
        {
            if (name.empty() || name.size() > 255 || !std::isalpha(static_cast<unsigned char>(name[0])))
                return false;

            return std::ranges::all_of(name, [](const char c) {
                return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '.' || c == '-' || c == '/';
            });
        }

        //  비교를 위해 키 순서로 정렬. 중복 시 false
        bool NormalizeAttributes(Attributes& attributes)
        {
            std::ranges::sort(attributes, {}, &Attribute::key);
            return std::ranges::adjacent_find(attributes, {}, &Attribute::key) == attributes.end();
        }
    }

    template <typename T, typename... Args>
    T Registry::AddSeries(MetricInfo info, Args&&... args)
    {
        using Storage = typename T::Storage;

        if constexpr (!Enabled)
            return {};

        //  오류: 잘못된 이름
        if (!IsValidName(info.name))
        {
            LogError(info.name, "invalid name");
            return {};
        }

        //  오류: 중복 속성이 있음
        if (!NormalizeAttributes(info.attributes))
        {
            LogError(info.name, "duplicate attribute key");
            return {};
        }

        std::lock_guard guard(StoragesLock());

        for (const auto& storage : GetStorages())
        {
            const auto& existing = storage->info;
            if (existing.name != info.name)
                continue;

            //  오류: 같은 이름의 다른 타입을 등록할 수 없음.
            if (existing.type != info.type || existing.unit != info.unit || existing.bounds != info.bounds)
            {
                LogError(info.name, "already registered as a different kind");
                return {};
            }

            //  속성에 따라 다른 시리즈가 나올 수 있음
            if (existing.attributes != info.attributes)
                continue;

            //  오류: 콜백은 기존 스토리지를 같이 사용하면 수집할 때 어떤 값을 써야 할지 모호해진다.
            if (existing.type == MetricType::CallbackGauge)
            {
                LogError(info.name, "series already registered");
                return {};
            }

            return T(static_cast<Storage*>(storage.get()));
        }

        GetStorages().push_back(std::make_unique<Storage>(std::move(info), std::forward<Args>(args)...));
        return T(static_cast<Storage*>(GetStorages().back().get()));
    }

    Counter Registry::CreateCounter(std::string_view name, std::string_view description, Attributes attributes)
    {
        return AddSeries<Counter>({
            .name = std::string(name),
            .description = std::string(description),
            .type = MetricType::Counter,
            .attributes = std::move(attributes),
        });
    }

    Gauge Registry::CreateGauge(std::string_view name, std::string_view description, Attributes attributes,
                                const Unit unit)
    {
        return AddSeries<Gauge>({
            .name = std::string(name),
            .description = std::string(description),
            .type = MetricType::Gauge,
            .unit = unit,
            .attributes = std::move(attributes),
        });
    }

    DoubleGauge Registry::CreateDoubleGauge(std::string_view name, std::string_view description,
                                            Attributes attributes, const Unit unit)
    {
        return AddSeries<DoubleGauge>({
            .name = std::string(name),
            .description = std::string(description),
            .type = MetricType::DoubleGauge,
            .unit = unit,
            .attributes = std::move(attributes),
        });
    }

    Histogram Registry::CreateTimeHistogram(std::string_view name, std::string_view description,
                                            Attributes attributes)
    {
        const auto bounds = Histogram::LatencyUsBounds();
        return AddSeries<Histogram>({
            .name = std::string(name),
            .description = std::string(description),
            .type = MetricType::Histogram,
            .unit = Unit::Microseconds,
            .attributes = std::move(attributes),
            .bounds = {bounds.begin(), bounds.end()},
        });
    }

    Histogram Registry::CreateHistogram(std::string_view name, std::string_view description,
                                        Bounds bounds, Attributes attributes)
    {
        if (bounds.empty() || std::ranges::adjacent_find(bounds, std::greater_equal{}) != bounds.end())
        {
            LogError(name, "bounds must be non-empty and strictly increasing");
            return {};
        }

        return AddSeries<Histogram>({
            .name = std::string(name),
            .description = std::string(description),
            .type = MetricType::Histogram,
            .attributes = std::move(attributes),
            .bounds = {bounds.begin(), bounds.end()},
        });
    }

    CallbackGauge Registry::CreateCallbackGauge(std::string_view name, std::string_view description,
                                                Attributes attributes, std::function<int64()> callback)
    {
        if (callback == nullptr)
            return {};

        return AddSeries<CallbackGauge>({
            .name = std::string(name),
            .description = std::string(description),
            .type = MetricType::CallbackGauge,
            .attributes = std::move(attributes),
        }, std::move(callback));
    }

    void Registry::Remove(const StorageBase* storage)
    {
        std::lock_guard guard(StoragesLock());
        std::erase_if(GetStorages(), [storage](const auto& stored) { return stored.get() == storage; });
    }

    /////////////////////
    //      Init
    /////////////////////

    namespace
    {

        void ExportTo(Exporter& exporter)
        {
            {
                std::lock_guard guard(StoragesLock());

                exporter.Begin(gStart, SystemClock::Now());
                for (const auto& storage : GetStorages())
                    storage->WriteTo(exporter);
            }
            exporter.Flush();
        }

        class ExportRunner
        {
        public:
            //  이미 실행 중이면 false
            static bool Start(std::vector<ExporterPtr> exporters)
            {
                std::lock_guard guard(_instanceLock);
                if (_instance != nullptr)
                    return false;

                _instance.reset(new ExportRunner(std::move(exporters)));
                return true;
            }

            static void Stop()
            {
                std::unique_ptr<ExportRunner> runner;
                {
                    std::lock_guard guard(_instanceLock);
                    runner = std::move(_instance);
                }
            }

            ~ExportRunner()
            {
                _stop.release();
                _thread.join();

                //  종료 전 남은 데이터 전송
                for (const auto& exporter : _exporters)
                {
                    ExportTo(*exporter);
                    exporter->Shutdown();
                }
            }

            ExportRunner(const ExportRunner& other) = delete;
            ExportRunner& operator=(const ExportRunner& other) = delete;

        private:
            explicit ExportRunner(std::vector<ExporterPtr> exporters)
                : _exporters(std::move(exporters))
            {
                auto now = SteadyClock::Now();
                for (const auto& exporter : _exporters)
                    _wakeupTimes.push_back(now + exporter->Interval());

                _thread = std::thread([this] { Run(); });
            }

            void Run()
            {
                while (true)
                {
                    auto nextWakeUp = *std::ranges::min_element(_wakeupTimes);
                    if (_stop.try_acquire_until(nextWakeUp))
                        return;

                    for (size_t i = 0; i < _exporters.size(); ++i)
                    {
                        auto now = SteadyClock::Now();
                        if (now < _wakeupTimes[i])
                            continue;

                        ExportTo(*_exporters[i]);

                        //  늦게 깨어나도 밀린 걸 보내지 않는다. 누적 값이라 누락되지 않는다. 
                        //  Export가 오래 걸릴 걸 생각해서 SteadyClock::Now() 사용함. 
                        _wakeupTimes[i] = std::max(_wakeupTimes[i] + _exporters[i]->Interval(), SteadyClock::Now());
                    }
                }
            }

            std::vector<ExporterPtr> _exporters;
            std::vector<SteadyClock::TimePoint> _wakeupTimes;
            //  시간까지 기다리거나 외부에서 종료 요청오면 나가는 용도
            //  기존에는 condition_variable을 썼으나, 세마포어가 더 간단하게 사용할 수 있음
            std::binary_semaphore _stop{0};
            std::thread _thread;

            static inline std::mutex _instanceLock;
            static inline std::unique_ptr<ExportRunner> _instance;
        };
    }

    Config ConfigFromEnv(const std::string_view serviceName)
    {
        Config config{.serviceName = std::string(serviceName)};
        if (const char* endpoint = std::getenv("OTEL_EXPORTER_OTLP_METRICS_ENDPOINT"))
        {
            config.useOtlp = true;
            config.otlpEndpoint = endpoint;
        }

        if (const char* host = std::getenv("HOSTNAME"))
            config.serviceInstanceId = host;

        return config;
    }

    //  오류 문자열은 std::string으로 감싸야 한다. const char*는 bool 생성자로 변환된다.
    Result<std::string> Init(const Config& config)
    {
        if constexpr (!Enabled)
            return true;

        if (config.otlpInterval <= std::chrono::milliseconds::zero() ||
            config.consoleInterval <= std::chrono::milliseconds::zero())
            return std::string("metrics export interval must be positive");

        std::vector<ExporterPtr> exporters;
        bool useConsole = config.useConsole;
        if (config.useOtlp)
        {
#ifdef NETLIB_ENABLE_OTEL
            exporters.push_back(CreateOtlpExporter(config));
#else
            if (config.requireOtlp)
                return std::string("NetLib was built without NETLIB_ENABLE_OTEL");

            fmt::print(stderr, "[metrics] otlp unavailable (built without NETLIB_ENABLE_OTEL), falling back to console\n");
            useConsole = true;
#endif
        }

        if (useConsole)
            exporters.push_back(CreateConsoleExporter(config));

        if (config.useConsoleMonitor)
            exporters.push_back(CreateConsoleMonitorExporter(config));

        if (exporters.empty())
            return true;

        if (!ExportRunner::Start(std::move(exporters)))
            return std::string("metrics already initialized");

        return true;
    }

    void Shutdown()
    {
        ExportRunner::Stop();
    }
}
