#include "SockAddr.h"

#ifdef _WIN32
#include "MyWindows.h"
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#endif

#include <array>
#include <format>

namespace
{
template <typename CharT>
std::optional<Ip> ParseIp(const std::basic_string_view<CharT> str)
{
    std::array<psh::uint8, 4> octets{};
    std::size_t pos = 0;

    for (std::size_t i = 0; i < octets.size(); ++i)
    {
        if (i > 0)
        {
            if (pos >= str.size() || str[pos] != CharT('.'))
                return std::nullopt;
            ++pos;
        }

        int value = 0;
        int digits = 0;
        while (pos < str.size() && CharT('0') <= str[pos] && str[pos] <= CharT('9'))
        {
            value = value * 10 + (str[pos] - CharT('0'));
            if (++digits > 3 || value > 255)
                return std::nullopt;
            ++pos;
        }
        if (digits == 0)
            return std::nullopt;

        octets[i] = static_cast<psh::uint8>(value);
    }

    if (pos != str.size())
        return std::nullopt;

    return Ip{octets[0], octets[1], octets[2], octets[3]};
}
}

std::optional<Ip> Ip::Parse(const std::string_view str)
{
    return ParseIp(str);
}

std::optional<Ip> Ip::Parse(const std::wstring_view str)
{
    return ParseIp(str);
}

std::string Ip::ToString() const
{
    return std::format("{}.{}.{}.{}", _value >> 24, _value >> 16 & 0xFF, _value >> 8 & 0xFF, _value & 0xFF);
}

std::wstring Ip::ToWString() const
{
    return std::format(L"{}.{}.{}.{}", _value >> 24, _value >> 16 & 0xFF, _value >> 8 & 0xFF, _value & 0xFF);
}

SockAddr::SockAddr(const sockaddr_in& native)
    : _ip(ntohl(native.sin_addr.s_addr)), _port(ntohs(native.sin_port))
{
}

sockaddr_in SockAddr::ToNative() const
{
    sockaddr_in native{};
    native.sin_family = AF_INET;
    native.sin_addr.s_addr = htonl(_ip.ToUint());
    native.sin_port = htons(_port);
    return native;
}

std::string SockAddr::ToString() const
{
    return std::format("{}:{}", _ip.ToString(), _port);
}

std::wstring SockAddr::ToWString() const
{
    return std::format(L"{}:{}", _ip.ToWString(), _port);
}
