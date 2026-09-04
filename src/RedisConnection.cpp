#include "RedisConnection.h"
#include <optional>

#include "Logger.h"
#include "CoreGlobal.h"
#include <cpp_redis/cpp_redis.h>

struct RedisConnection::redisImple
{
    cpp_redis::client _conn;
    cpp_redis::connect_state _status;
};

RedisConnection::RedisConnection(const psh::LPCSTR ip, const psh::uint32 port, const psh::uint32 timeout
  , const psh::int32 maxReconnect, const psh::uint32 reconnectInterval): _imple{std::make_unique<redisImple>()}
{
    _imple->_conn.connect(ip, port, nullptr, timeout, maxReconnect, reconnectInterval);
}

RedisConnection::~RedisConnection()
{
    _imple->_conn.disconnect(false);
}

bool RedisConnection::Set(const std::string& key, const std::string& value)
{
    auto ret = _imple->_conn.set(key, value);
    _imple->_conn.sync_commit();

    return ret.get().ok();
}

bool RedisConnection::Del(const std::string& key)
{
    std::string k(key.begin(), key.end());

    auto ret = _imple->_conn.del({"DEL", key});
    _imple->_conn.sync_commit();

    return ret.get().ok();
}



bool RedisConnection::SetExpired(const std::string& key, const std::string& value, const int expireSec)
{
    auto ret = _imple->_conn.set_advanced(key, value, true, expireSec);
    _imple->_conn.sync_commit();

    return ret.get().ok();
}

std::optional<String> RedisConnection::Get(const std::string& key)
{
    auto result = _imple->_conn.get(key.c_str());
    _imple->_conn.sync_commit();

    const auto rep = result.get();

    if (!rep.ok())
    {
        return {};
    }

    if (rep.is_null())
    {
        String wKey(key.length(), L'\n');
        wKey.assign(key.begin(), key.end());
        gLogger->Write(L"Redis", Logger::LogLevel::Debug, L"invalid key %s", wKey.c_str());

        return {};
    }
    auto cRet = rep.as_string();

    String ret(cRet.begin(), cRet.end());

    return ret;
}
