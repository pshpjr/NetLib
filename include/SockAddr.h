#pragma once

#include "Types.h"

#include <cassert>
#include <optional>
#include <string>
#include <string_view>

class Ip
{
public:
    constexpr Ip() = default;

    constexpr Ip(psh::uint32 value)
        : _value(value)
    {
        
    }

    constexpr Ip(const psh::uint8 a, const psh::uint8 b, const psh::uint8 c, const psh::uint8 d)
        : _value(static_cast<psh::uint32>(a) << 24 | static_cast<psh::uint32>(b) << 16
                 | static_cast<psh::uint32>(c) << 8 | static_cast<psh::uint32>(d))
    {
    }

    //  "a.b.c.d" 형식. 잘못된 값이면 nullopt
    static std::optional<Ip> Parse(std::string_view str);
    static std::optional<Ip> Parse(std::wstring_view str);

    static constexpr Ip Any()
    {
        return {};
    }

    constexpr psh::uint32 ToUint() const
    {
        return _value;
    }

    std::string ToString() const;
    std::wstring ToWString() const;

    //  상위 maskBits(0~32)비트가 같으면 true
    constexpr bool IsSameSubnet(const Ip other, const int maskBits) const
    {
        assert(0 <= maskBits && maskBits <= 32);
        if (maskBits <= 0)
            return true;

        const psh::uint32 mask = maskBits >= 32 ? 0xFFFF'FFFFu : ~(0xFFFF'FFFFu >> maskBits);
        return (_value & mask) == (other._value & mask);
    }

    constexpr bool operator==(const Ip& other) const = default;

private:
    psh::uint32 _value = 0;
};

//  IPv4 주소 + 포트
struct sockaddr_in;

class SockAddr
{
public:
    constexpr SockAddr() = default;

    constexpr SockAddr(const Ip ip, const psh::uint16 port)
        : _ip(ip), _port(port)
    {
    }

    explicit SockAddr(const sockaddr_in& native);

    sockaddr_in ToNative() const;

    constexpr Ip GetIp() const
    {
        return _ip;
    }

    constexpr psh::uint16 GetPort() const
    {
        return _port;
    }

    //  "a.b.c.d:port"
    std::string ToString() const;
    std::wstring ToWString() const;

    constexpr bool operator==(const SockAddr& other) const = default;

private:
    Ip _ip{};
    psh::uint16 _port = 0;
};
