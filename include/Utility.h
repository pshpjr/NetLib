//
// Created by Park on 24. 8. 22.
//

#ifndef UTILITY_H
#define UTILITY_H
#include <cstdint>
#include <stdexcept>
#include <string>
#include <variant>

#ifndef _WIN32
#include <pthread.h>
#endif

namespace psh::util
{
    std::string WToS(const std::wstring &wstr);

    void set_thread_name(const char *threadName);
}

namespace psh::util
{
    template<typename T, typename E>
    class Result {
    public:
        using valueType = T;
        using errorType = E;

        enum class State {
            HasValue, HasError
        };

        Result(const valueType &value)
            : _state(State::HasValue)
          , _variant(value)
        {
        }

        Result(valueType &&value)
            : _state(State::HasValue)
          , _variant(std::move(value))
        {
        }

        Result(const errorType &error)
            : _state(State::HasError)
          , _variant(error)
        {
        }

        Result(errorType &&error)
            : _state(State::HasError)
          , _variant(std::move(error))
        {
        }

        operator bool() const
        {
            return _state == State::HasValue;
        }

        bool HasValue() const
        {
            return _state == State::HasValue;
        }

        valueType &Value()
        {
            if (_state == State::HasValue)
            {
                return std::get<valueType>(_variant);
            }
            else
            {
                throw std::logic_error("Error: Result does not contain a value");
            }
        }

        bool HasError() const
        {
            return _state == State::HasError;
        }

        errorType &Error()
        {
            if (_state == State::HasError)
            {
                return std::get<errorType>(_variant);
            }
            else
            {
                throw std::logic_error("Error: Result does not contain an error");
            }
        }

    private:
        State _state;
        std::variant<valueType, errorType> _variant;
    };
}

namespace psh::util
{
#ifdef _WIN32
    using TlsKey = std::uint32_t;
#else
    using TlsKey = pthread_key_t;
#endif

    bool TlsAlloc(TlsKey& outKey);
    void TlsFree(TlsKey key);
    bool TlsSet(TlsKey key, void* value);
    void* TlsGet(TlsKey key);
}

#endif //UTILITY_H


