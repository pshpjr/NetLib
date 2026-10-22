#pragma once

#include "Metrics.h"
#include "MetricsExporter.h"

#include <atomic>
#include <functional>
#include <vector>

//  NetLib 내부 전용. 핸들이 가리키는 저장소. Registry가 소유한다.
namespace psh::metrics
{
    struct StorageBase
    {
        explicit StorageBase(MetricInfo info)
            : info(std::move(info))
        {
        }

        virtual ~StorageBase() = default;

        //  현재 값을 exporter에 쓴다. 등록 잠금을 잡고 호출된다.
        virtual void WriteTo(Exporter& exporter) const = 0;

        MetricInfo info;
    };

    struct Counter::Storage : StorageBase
    {
        using StorageBase::StorageBase;

        void WriteTo(Exporter& exporter) const override
        {
            exporter.WriteCounter(info, value.load(std::memory_order_relaxed));
        }

        std::atomic<uint64> value{0};
    };

    struct Gauge::Storage : StorageBase
    {
        using StorageBase::StorageBase;

        void WriteTo(Exporter& exporter) const override
        {
            exporter.WriteGauge(info, value.load(std::memory_order_relaxed));
        }

        std::atomic<int64> value{0};
    };

    struct CallbackGauge::Storage : StorageBase
    {
        Storage(MetricInfo info, std::function<int64()> callback)
            : StorageBase(std::move(info))
            , callback(std::move(callback))
        {
        }

        void WriteTo(Exporter& exporter) const override
        {
            exporter.WriteGauge(info, callback());
        }

        std::function<int64()> callback;
    };

    struct Histogram::Storage : StorageBase
    {
        explicit Storage(MetricInfo info)
            : StorageBase(std::move(info))
            , counts(this->info.bounds.size() + 1)
        {
        }

        //  버킷을 하나씩 읽으므로 기록 중인 값 하나 정도는 counts와 sum에 어긋나게 반영될 수 있다.
        void WriteTo(Exporter& exporter) const override
        {
            std::vector<uint64> values;
            values.reserve(counts.size());
            for (const auto& count : counts)
                values.push_back(count.load(std::memory_order_relaxed));

            exporter.WriteHistogram(info, values, sum.load(std::memory_order_relaxed));
        }

        //  info.bounds[i] 이하의 값 개수. 마지막은 +Inf 버킷이다.
        std::vector<std::atomic<uint64>> counts;
        std::atomic<int64> sum{0};
    };
}
