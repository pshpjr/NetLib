#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace psh::util
{
    static_assert(sizeof(char16_t) == 2);

    namespace detail
    {
        // wchar_t의 2바이트/4바이트 표현을 모두 처리한다.
        template <typename WideChar>
        std::u16string EncodeUtf16(std::basic_string_view<WideChar> input)
        {
            static_assert(sizeof(WideChar) == 2 || sizeof(WideChar) == 4);
            std::u16string result;
            result.reserve(input.size());
            for (std::size_t i = 0; i < input.size(); ++i)
            {
                auto point = static_cast<std::uint32_t>(input[i]);
                if constexpr (sizeof(WideChar) == 2)
                {
                    if (point >= 0xD800 && point <= 0xDBFF)
                    {
                        if (++i == input.size())
                            throw std::invalid_argument("Incomplete UTF-16 surrogate pair");
                        const auto low = static_cast<std::uint32_t>(input[i]);
                        if (low < 0xDC00 || low > 0xDFFF)
                            throw std::invalid_argument("Invalid UTF-16 surrogate pair");
                        result.push_back(static_cast<char16_t>(point));
                        result.push_back(static_cast<char16_t>(low));
                        continue;
                    }
                }
                if (point > 0x10FFFF || (point >= 0xD800 && point <= 0xDFFF))
                    throw std::invalid_argument("Invalid Unicode code point");
                if (point <= 0xFFFF)
                    result.push_back(static_cast<char16_t>(point));
                else
                {
                    point -= 0x10000;
                    result.push_back(static_cast<char16_t>(0xD800 + (point >> 10)));
                    result.push_back(static_cast<char16_t>(0xDC00 + (point & 0x3FF)));
                }
            }
            return result;
        }

        template <typename WideChar>
        std::basic_string<WideChar> DecodeUtf16(std::u16string_view input)
        {
            static_assert(sizeof(WideChar) == 2 || sizeof(WideChar) == 4);
            std::basic_string<WideChar> result;
            result.reserve(input.size());
            for (std::size_t i = 0; i < input.size(); ++i)
            {
                const auto high = static_cast<std::uint32_t>(input[i]);
                if (high >= 0xD800 && high <= 0xDBFF)
                {
                    if (++i == input.size())
                        throw std::invalid_argument("Incomplete UTF-16 surrogate pair");
                    const auto low = static_cast<std::uint32_t>(input[i]);
                    if (low < 0xDC00 || low > 0xDFFF)
                        throw std::invalid_argument("Invalid UTF-16 surrogate pair");
                    if constexpr (sizeof(WideChar) == 2)
                    {
                        result.push_back(static_cast<WideChar>(high));
                        result.push_back(static_cast<WideChar>(low));
                    }
                    else
                        result.push_back(static_cast<WideChar>(
                            0x10000 + ((high - 0xD800) << 10) + low - 0xDC00));
                }
                else
                {
                    if (high >= 0xDC00 && high <= 0xDFFF)
                        throw std::invalid_argument("Invalid UTF-16 surrogate pair");
                    result.push_back(static_cast<WideChar>(high));
                }
            }
            return result;
        }
    }

    inline std::u16string WideToUtf16(std::wstring_view input)
    {
        return detail::EncodeUtf16(input);
    }

    inline std::wstring Utf16ToWide(std::u16string_view input)
    {
        return detail::DecodeUtf16<wchar_t>(input);
    }

    inline void WriteUtf16LE(char* destination, std::u16string_view input)
    {
        for (const auto unit : input)
        {
            *destination++ = static_cast<char>(unit & 0xFF);
            *destination++ = static_cast<char>(unit >> 8);
        }
    }

    inline std::u16string ReadUtf16LE(std::string_view bytes)
    {
        if (bytes.size() % 2 != 0)
            throw std::invalid_argument("Odd UTF-16 byte count");
        std::u16string result(bytes.size() / 2, u'\0');
        for (std::size_t i = 0; i < result.size(); ++i)
            result[i] = static_cast<char16_t>(static_cast<unsigned char>(bytes[2 * i]) |
                (static_cast<unsigned char>(bytes[2 * i + 1]) << 8));
        return result;
    }
}
