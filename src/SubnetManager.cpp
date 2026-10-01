#include "SubnetManager.h"
#include "CoreGlobal.h"
#include "Logger.h"
#include "stdafx.h"


SubnetManager::SubnetManager()
{
    _subnetParser.Init(L"subnetSetting.toml");
    LoadSubnets();
}

bool SubnetManager::IsLan(const SockAddr& info)
{
    return FindSubnet(_lanSubnet, info).has_value();
}

bool SubnetManager::IsWan(const SockAddr& info)
{
    return FindSubnet(_wanSubnet, info).has_value();
}

std::optional<SubnetManager::SubnetInfo> SubnetManager::FindSubnet(Vector<SubnetInfo>& group, const SockAddr info)
{
    const auto result = std::ranges::find_if(group
        , [info](const SubnetInfo& data)
        {
            return data.address.IsSameSubnet(info.GetIp(), data.subnetSize);
        });

    if (result == group.end())
    {
        return {};
    }
    return *result;
}

void SubnetManager::LoadSubnets()
{
    _subnetParser.Init(L"subnetSetting.toml");
    LoadSubnetGroup(L"LAN", _lanSubnet);
    LoadSubnetGroup(L"WAN", _wanSubnet);
}

void SubnetManager::LoadSubnetGroup(String group, Vector<SubnetInfo>& container)
{
    group += L".";
    int len;

    _subnetParser.GetValue(group + L"len", len);

    const String ipString = group + L"ip_";
    const String subnetString = group + L"mask_";
    String portString = group + L"port_";

    for (int i = 0; i < len; i++)
    {
        String ip;
        char subnet = 0;

        _subnetParser.GetValue(subnetString + std::to_wstring(i), subnet);
        _subnetParser.GetValue(ipString + std::to_wstring(i), ip);

        const auto address = Ip::Parse(ip);
        if (!address.has_value())
        {
            gLogger->Write(L"SubnetManager", Logger::LogLevel::Error, L"Invalid subnet ip : %s", ip.c_str());
            continue;
        }

        container.push_back({subnet, *address, ip});
    }

    std::ranges::sort(container, [](const SubnetInfo& d1, const SubnetInfo& d2)
    {
        return d1.subnetSize > d2.subnetSize;
    });
}
