#include "MetricHandles.h"

#include "MetricStorage.h"

#include <utility>

namespace psh::metrics
{
    /////////////////////
    //      Counter
    /////////////////////

    Counter::Counter(Storage* storage)
        : _storage(storage)
    {
    }

    void Counter::AddImpl(const uint64 value) const
    {
        if (_storage == nullptr)
            return;

        _storage->value.fetch_add(value, std::memory_order_relaxed);
    }

    const MetricInfo* Counter::Info() const
    {
        return _storage != nullptr ? &_storage->info : nullptr;
    }

    /////////////////////
    //      Gauge
    /////////////////////

    Gauge::Gauge(Storage* storage)
        : _storage(storage)
    {
    }

    void Gauge::SetImpl(const int64 value) const
    {
        if (_storage == nullptr)
            return;

        _storage->value.store(value, std::memory_order_relaxed);
    }

    void Gauge::AddImpl(const int64 delta) const
    {
        if (_storage == nullptr)
            return;

        _storage->value.fetch_add(delta, std::memory_order_relaxed);
    }

    const MetricInfo* Gauge::Info() const
    {
        return _storage != nullptr ? &_storage->info : nullptr;
    }

    /////////////////////
    //      DoubleGauge
    /////////////////////

    DoubleGauge::DoubleGauge(Storage* storage)
        : _storage(storage)
    {
    }

    void DoubleGauge::SetImpl(const double value) const
    {
        if (_storage == nullptr)
            return;

        _storage->value.store(value, std::memory_order_relaxed);
    }

    const MetricInfo* DoubleGauge::Info() const
    {
        return _storage != nullptr ? &_storage->info : nullptr;
    }

    /////////////////////
    //      CallbackGauge
    /////////////////////

    CallbackGauge::CallbackGauge(Storage* storage)
        : _storage(storage)
    {
    }

    //  내보내기와 같은 잠금을 잡으므로, 내보내는 중이면 끝날 때까지 기다린 뒤 지운다.
    CallbackGauge::~CallbackGauge()
    {
        if (_storage == nullptr)
            return;

        Registry::Remove(_storage);
    }

    CallbackGauge::CallbackGauge(CallbackGauge&& other) noexcept
        : _storage(std::exchange(other._storage, nullptr))
    {
    }

    CallbackGauge& CallbackGauge::operator=(CallbackGauge&& other) noexcept
    {
        if (this != &other)
        {
            CallbackGauge released(std::move(*this));
            _storage = std::exchange(other._storage, nullptr);
        }
        return *this;
    }

    const MetricInfo* CallbackGauge::Info() const
    {
        return _storage != nullptr ? &_storage->info : nullptr;
    }
}
