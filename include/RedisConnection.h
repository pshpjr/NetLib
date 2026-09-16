#pragma once
#include "Types.h"

#include <optional>
class RedisConnection
{
public:
	//  MaxReconnect, ReconnectInterval은 사용하지 않음
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

private:
    struct redisImple;
    std::unique_ptr<redisImple> _imple;
};
