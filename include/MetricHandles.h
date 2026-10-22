#pragma once

#include "Types.h"

#include <functional>
#include <string>
#include <vector>

namespace psh::metrics
{
#ifdef NETLIB_ENABLE_METRICS
    inline constexpr bool Enabled = true;
#else
    inline constexpr bool Enabled = false;
#endif

    struct Attribute
    {
        std::string key;
        std::string value;

        bool operator==(const Attribute& other) const = default;
    };

    using Attributes = std::vector<Attribute>;

    enum class MetricType : uint8
    {
        Counter,
        Gauge,
        CallbackGauge,
        Histogram,
    };

    //  시간은 µs로 기록하고 내보낼 때 초 단위(s)로 변환한다.
    enum class Unit : uint8
    {
        None,
        Microseconds,
    };

    //  등록한 계측기의 정보. 같은 이름이면 type, unit, bounds가 같아야 한다.
    struct MetricInfo
    {
        std::string name;
        std::string description;
        MetricType type = MetricType::Counter;
        Unit unit = Unit::None;
        Attributes attributes;
        //  Histogram 버킷 경계. 다른 종류는 비어 있다.
        std::vector<int64> bounds;
    };

    //  핸들은 레지스트리가 소유한 Storage를 가리키기만 한다. Info()는 빈 핸들이면 nullptr.
    class Counter
    {
        friend class Registry;

    public:
        struct Storage;

        Counter() = default;

        void Add(uint64 value = 1) const { if constexpr (Enabled) { AddImpl(value); } }

        [[nodiscard]] const MetricInfo* Info() const;

    private:
        explicit Counter(Storage* storage);

        void AddImpl(uint64 value) const;

        Storage* _storage = nullptr;
    };

    //  큐 길이처럼 증감하거나 덮어쓰는 값에 사용한다.
    class Gauge
    {
        friend class Registry;

    public:
        struct Storage;

        Gauge() = default;

        void Set(int64 value) const { if constexpr (Enabled) { SetImpl(value); } }
        void Add(int64 delta) const { if constexpr (Enabled) { AddImpl(delta); } }

        [[nodiscard]] const MetricInfo* Info() const;

    private:
        explicit Gauge(Storage* storage);

        void SetImpl(int64 value) const;
        void AddImpl(int64 delta) const;

        Storage* _storage = nullptr;
    };

    //  콜백이 참조하는 객체보다 먼저 소멸되어야 함
    class CallbackGauge
    {
        friend class Registry;

    public:
        struct Storage;

        CallbackGauge() = default;
        ~CallbackGauge();

        CallbackGauge(const CallbackGauge& other) = delete;
        CallbackGauge& operator=(const CallbackGauge& other) = delete;

        CallbackGauge(CallbackGauge&& other) noexcept;
        CallbackGauge& operator=(CallbackGauge&& other) noexcept;

        [[nodiscard]] const MetricInfo* Info() const;

    private:
        explicit CallbackGauge(Storage* storage);

        Storage* _storage = nullptr;
    };
}
