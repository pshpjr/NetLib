#pragma once

#include "MetricHandles.h"
#include "MetricHistogram.h"

#include <chrono>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace psh::metrics
{
    struct StorageBase;

    //  모든 Storage를 소유한다. 등록, 내보내기, CallbackGauge 해제가 같은 잠금을 쓴다.
    class Registry
    {
        friend class CallbackGauge;

    public:
        //  이름은 OTel 계측기 이름 규칙([A-Za-z][A-Za-z0-9_.\-/]*, 최대 255자)을 따른다.
        //  잘못된 이름이나 같은 이름의 다른 종류·경계 등록은 빈 핸들을 돌려주고 기록을 무시한다.
        static Counter CreateCounter(std::string_view name, std::string_view description, Attributes attributes = {});

        static Gauge CreateGauge(std::string_view name, std::string_view description, Attributes attributes = {});

        //  µs로 기록하고 초로 내보낸다. 경계는 Histogram::LatencyUsBounds()다.
        static Histogram CreateTimeHistogram(std::string_view name, std::string_view description,
                                             Attributes attributes = {});

        //  단위 없음. 경계를 직접 지정한다.
        static Histogram CreateHistogram(std::string_view name, std::string_view description,
                                         Bounds bounds, Attributes attributes = {});

        //  콜백이 참조하는 객체보다 먼저 소멸되어야 함
        [[nodiscard]] static CallbackGauge CreateCallbackGauge(std::string_view name, std::string_view description,
                                                               Attributes attributes, std::function<int64()> callback);

    private:
        //  같은 이름·속성이 이미 있으면 그 저장소의 핸들을, 없으면 새로 만들어 돌려준다. 실패하면 빈 핸들.
        //  args는 새로 만들 때 T::Storage 생성자의 info 다음 인자로 넘어간다.
        template <typename T, typename... Args>
        static T AddSeries(MetricInfo info, Args&&... args);

        static void Remove(const StorageBase* storage);
    };

    struct Config
    {
        std::string serviceName;
        std::string serviceVersion;
        std::string serviceInstanceId;
        std::string environment;
        Attributes resourceAttributes;

        //  둘 다 끄면 기록만 하고 내보내지 않는다.
        bool useConsole = false;
        bool useOtlp = false;

        std::string otlpEndpoint = "http://localhost:4318/v1/metrics";
        std::chrono::milliseconds otlpInterval{10000};
        std::chrono::milliseconds otlpTimeout{3000};
        std::chrono::milliseconds consoleInterval{1000};

        //  useOtlp 켰는데 otlp을 못 쓰면 실패 처리
        //  배포 시 실수 방지용
        bool requireOtlp = false;
    };

    //  exporter를 만들고 내보내기 스레드를 시작한다. 실패하면 이유를 Error()로 돌려준다.
    Result<std::string> Init(const Config& config);

    //  마지막 값을 내보낸 뒤 스레드를 종료한다. 정상 종료 경로에서 호출한다.
    void Shutdown();
}
