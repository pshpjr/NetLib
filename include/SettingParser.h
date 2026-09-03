#pragma once
#include <algorithm>
#include <optional>

#include "Types.h"
#include "Container.h"
#include "Macro.h"

/*
	파일 형식은

	그룹 :
	{
	키 : 값
	키 : 값
	}
	값은 띄어쓰기 단위로 구별한다. 
	값은 {}로 감싸면 안 된다
	 
 */


class SettingParser
{
public:
    SettingParser() : _groupsName(MAXGROUPSIZE)
                    , _settingsContainer(MAXGROUPSIZE)
    {
    }

    enum class ErrType
    {
        Success
        , FileOpenErr
        , FileReadErr
        , OpErr
        ,
    };

    void Init(psh::LPCWSTR location = L"serverSetting.txt");

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
            std::string str(resultStr.length(), 0);
            std::ranges::transform(resultStr.begin(), resultStr.end(), str.begin(),
                [](const wchar_t c)
                {
                    return static_cast<char>(c);
                });
            value = move(str);
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

    //void GetValue(const String name, OUT LPWSTR value);
    //void GetValue(const String name, OUT String& value);
    //void GetValue(const String name, OUT std::string& value);

    ~SettingParser() = default;

private:
    
    std::optional<String> GetValueImpl(const String& name) const;
    void LoadSetting(psh::LPCWSTR location);
    void Parse();
    bool GetTok(OUT psh::LPWSTR word);

private:
    static constexpr int MAX_WORD_SIZE = 100;
    static constexpr int MAXERRLEN = 100;
    static constexpr int MAXGROUPSIZE = 10;

    psh::LPWSTR _buffer = nullptr;

    size_t _bufferIndex = 0;
    size_t bufferSize = 0;

    psh::int32 _groupIndex = -1;
    Vector<String> _groupsName;
    
    Vector<HashMap<String, String>> _settingsContainer;
};
