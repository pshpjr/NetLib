#pragma once

namespace psh
{
template <typename T, typename E> class ResultImpl
{
    static_assert(!std::is_same_v<T, E>, "Value and error types must differ.");

public:
    using valueType = T;
    using errorType = E;

    enum class State
    {
        HasValue,
        HasError
    };

    ResultImpl(const valueType &value) : _state(State::HasValue), _variant(std::in_place_index<0>, value)
    {
    }

    ResultImpl(valueType &&value) : _state(State::HasValue), _variant(std::in_place_index<0>, std::move(value))
    {
    }

    ResultImpl(const errorType &error) : _state(State::HasError), _variant(std::in_place_index<1>, error)
    {
    }

    ResultImpl(errorType &&error) : _state(State::HasError), _variant(std::in_place_index<1>, std::move(error))
    {
    }

    operator bool() const
    {
        return HasValue();
    }

    bool HasValue() const
    {
        return _state == State::HasValue;
    }

    bool HasError() const
    {
        return _state == State::HasError;
    }

    valueType &Value()
    {
        if (!HasValue())
            throw std::logic_error("Error: Result does not contain a value");

        return std::get<0>(_variant);
    }

    const valueType &Value() const
    {
        if (!HasValue())
            throw std::logic_error("Error: Result does not contain a value");

        return std::get<0>(_variant);
    }

    errorType &Error()
    {
        if (!HasError())
            throw std::logic_error("Error: Result does not contain an error");

        return std::get<1>(_variant);
    }

    const errorType &Error() const
    {
        if (!HasError())
            throw std::logic_error("Error: Result does not contain an error");

        return std::get<1>(_variant);
    }

private:
    State _state;
    std::variant<valueType, errorType> _variant;
};

template <typename T, typename E = void>
using Result = std::conditional_t<std::is_void_v<E>, ResultImpl<bool, T>, ResultImpl<T, E>>;
} // namespace psh