#include "RedisConnection.h"
#include <chrono>

#include "CLogger.h"
#include "CoreGlobal.h"
#include <sw/redis++/redis++.h>

struct RedisConnection::redisImple
{
    redisImple(const psh::LPCSTR ip, const psh::uint32 port, const psh::uint32 timeout)
        : _conn{[=]
        {
            sw::redis::ConnectionOptions options;
            options.host = ip;
            options.port = static_cast<int>(port);
            options.connect_timeout = std::chrono::milliseconds(timeout);
            return options;
        }()}
    {
    }

    sw::redis::Redis _conn;
};

// maxReconnect와 reconnectInterval은 redis-plus-plus에서 사용하지 않으며 API 호환을 위해 남겨둔다.
RedisConnection::RedisConnection(const psh::LPCSTR ip, const psh::uint32 port, const psh::uint32 timeout
  , const psh::int32 maxReconnect, const psh::uint32 reconnectInterval)
    : _imple{std::make_unique<redisImple>(ip, port, timeout)}
{

    static_cast<void>(maxReconnect);
    static_cast<void>(reconnectInterval);
}

RedisConnection::~RedisConnection() = default;

bool RedisConnection::Set(const std::string& key, const std::string& value)
{
    try
    {
        return _imple->_conn.set(key, value);
    }
    catch (const sw::redis::Error&)
    {
        return false;
    }
}

bool RedisConnection::Del(const std::string& key)
{
    try
    {
        _imple->_conn.del(key);
        return true;
    }
    catch (const sw::redis::Error&)
    {
        return false;
    }
}



bool RedisConnection::SetExpired(const std::string& key, const std::string& value, const int expireSec)
{
    try
    {
        return _imple->_conn.set(key, value, std::chrono::seconds(expireSec));
    }
    catch (const sw::redis::Error&)
    {
        return false;
    }
}

std::optional<String> RedisConnection::Get(const std::string& key)
{
    try
    {
        const auto result = _imple->_conn.get(key);

        if (!result)
        {
            const String wKey(key.begin(), key.end());
            gLogger->Write(L"Redis", CLogger::LogLevel::Debug, L"invalid key %s", wKey.c_str());

            return std::nullopt;
        }

        return String(result->begin(), result->end());
    }
    catch (const sw::redis::Error&)
    {
        return std::nullopt;
    }
}
