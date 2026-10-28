#include "MetricHistogram.h"

#include "MetricStorage.h"

#include <algorithm>
#include <array>

namespace psh::metrics
{
#ifdef NETLIB_ENABLE_METRICS
    Histogram::TimePoint::TimePoint(const SteadyClock::TimePoint time): _time(time)
    {
    }
#endif

    Bounds Histogram::LatencyUsBounds()
    {
        static constexpr std::array<int64, 33> bounds = {
            1,    2,     5,     10,    20,     50,     100,    200,    500,    1'000,
            2'000, 5'000, 10'000, 15'000, 20'000, 25'000, 30'000, 35'000, 40'000, 45'000,
            50'000, 60'000, 70'000, 80'000, 90'000, 100'000, 150'000, 200'000, 500'000, 1'000'000,
            2'000'000, 5'000'000, 10'000'000,
        };
        return bounds;
    }

    Histogram::Histogram(Storage* storage)
        : _storage(storage)
    {
    }

    void Histogram::RecordImpl(const int64 value) const
    {
        if (_storage == nullptr)
            return;

        //  상한 포함. 모든 경계보다 크면 마지막 +Inf 버킷에 들어간다.
        const auto& bounds = _storage->info.bounds;
        const auto index = std::ranges::lower_bound(bounds, value) - bounds.begin();
        _storage->counts[index].fetch_add(1, std::memory_order_relaxed);
        _storage->sum.fetch_add(value, std::memory_order_relaxed);
    }

    const MetricInfo* Histogram::Info() const
    {
        return _storage != nullptr ? &_storage->info : nullptr;
    }
}
