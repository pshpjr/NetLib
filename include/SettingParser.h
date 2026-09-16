#pragma once
#include "Container.h"
#include "Macro.h"
#include "Types.h"

#include <algorithm>
#include <optional>

/*
	파일 형식은 TOML이다.

	[그룹]
	키 = 값
 */


class SettingParser
{
    static constexpr int MAX_WORD_SIZE = 100;

public:

    enum class ErrType
    {
        Success
        , FileOpenErr
        , FileReadErr
        , OpErr
        ,
    };
    SettingParser() = default;

    //void GetValue(const String name, OUT LPWSTR value);
    //void GetValue(const String name, OUT String& value);
    //void GetValue(const String name, OUT std::string& value);

    ~SettingParser() = default;

    void Init(psh::LPCWSTR location = L"serverSetting.toml");

    template <typename T>
    void GetValue(const String name, T& value)
    {
        GetValueOrDefault(name, value);
    }

    template <typename T>
    void GetValueOrDefault(const String name, T& value, const String defaultValue = L"")
    {
        auto findResult = GetValueImpl(name);
        if (!findResult.has_value())
        {
            wprintf(L"Fail to Read Data : %s\n", name.c_str());
        }
        auto resultStr = findResult.value_or(defaultValue);

        if constexpr (std::is_integral_v<T>)
        {
            value = stoi(resultStr);
        }
        else if constexpr (std::is_floating_point_v<T>)
        {
            value = stod(resultStr);
        }
        else if constexpr (std::is_same_v<T, String>)
        {
            value = resultStr;
        }
        else if constexpr (std::is_same_v<T, std::string>)
        {
            value = psh::util::WToS(resultStr);
        }
        else if constexpr (std::is_same_v<T, psh::LPCWSTR>)
        {
            wcscpy_s(value, MAX_WORD_SIZE, resultStr.c_str());
        }
        else
        {
            static_assert(std::is_integral_v<T> || std::is_floating_point_v<T>, "Unsupported type");
        }
    }
private:
    std::optional<String> GetValueImpl(const String& name) const;
    void LoadSetting(psh::LPCWSTR location);
    HashMap<String, String> _settings;
};
