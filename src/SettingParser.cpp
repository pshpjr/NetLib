#include "SettingParser.h"

#include <cstring>
#include <format>
#include <string_view>

#include <iconv.hpp>
#include <toml++/toml.hpp>

#include "CoreGlobal.h"
#include "Logger.h"
#include "Utility.h"

namespace
{
String Utf8ToWide(const std::string_view input)
{
    std::string bytes;
    iconvpp::converter("WCHAR_T", "UTF-8").convert(std::string{input}, bytes);

    String result(bytes.size() / sizeof(wchar_t), L'\0');
    std::memcpy(result.data(), bytes.data(), bytes.size());
    return result;
}

std::optional<String> ToString(const toml::node& node)
{
    if (const auto value = node.value<std::string>())
        return Utf8ToWide(*value);
    if (const auto value = node.value<std::int64_t>())
        return std::to_wstring(*value);
    if (const auto value = node.value<double>())
        return std::format(L"{}", *value);
    if (const auto value = node.value<bool>())
        return *value ? L"1" : L"0";
    return std::nullopt;
}
}

void SettingParser::Init(const psh::LPCWSTR location)
{
    LoadSetting(location);
}

void SettingParser::LoadSetting(const psh::LPCWSTR location)
{
    _settings.clear();
    const auto config = toml::parse_file(psh::util::WToS(location));

    for (const auto& [groupName, groupNode] : config)
    {
        const auto* group = groupNode.as_table();
        if (group == nullptr)
            continue;

        for (const auto& [key, node] : *group)
        {
            if (auto value = ToString(node))
            {
                _settings.insert_or_assign(
                    Utf8ToWide(groupName.str()) + L"." + Utf8ToWide(key.str()),
                    std::move(*value));
            }
        }
    }
}

std::optional<String> SettingParser::GetValueImpl(const String& name) const
{
    const auto result = _settings.find(name);
    if (result == _settings.end())
    {
        const auto separator = name.find(L'.');
        const auto group = name.substr(0, separator);
        const auto key = separator == String::npos ? String{} : name.substr(separator + 1);
        gLogger->Write(L"SettingParser", Logger::LogLevel::Debug, L"%s %s", group.c_str(), key.c_str());
        return std::nullopt;
    }
    return result->second;
}
