#pragma once
#include <optional>
#include <sstream>
#include <type_traits>

#include "Types.h"
class RedisConnection
{
public:
    RedisConnection(const psh::LPCSTR ip
        , const psh::uint32 port
        , const psh::uint32 timeout = 0
        , const psh::int32 maxReconnect = 0
        , const psh::uint32 reconnectInterval = 0);

    ~RedisConnection();

    bool SetExpired(const std::string& key, const std::string& value, int expireSec);

    bool Set(const std::string& key, const std::string& value);

    bool Del(const std::string& key);

    std::optional<String> Get(const std::string& key);

    template <typename T>
    std::optional<T> Get(const std::string& key)
    {
        const auto value = Get(key);
        if (!value)
        {
            return std::nullopt;
        }

        if constexpr (std::is_same_v<T, String>)
        {
            return *value;
        }
        else if constexpr (std::is_default_constructible_v<T>
            && requires(std::wistringstream& stream, T& converted) { stream >> converted; })
        {
            T converted{};
            std::wistringstream stream(*value);
            stream >> converted;
            stream >> std::ws;

            if (stream.fail() || !stream.eof())
            {
                return std::nullopt;
            }

            return converted;
        }
        else
        {
            return std::nullopt;
        }
    }

private:
    struct redisImple;
    std::unique_ptr<redisImple> _imple;
};
