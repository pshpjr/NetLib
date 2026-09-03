#include "SubnetManager.h"

#include <algorithm>
#include "Socket.h"
#include "stdafx.h"


SubnetManager::SubnetManager()
{
    _subnetParser.Init(L"subnetSetting.txt");
    LoadSubnets();
}

bool SubnetManager::IsLan(const SockAddr_in& info)
{
    return FindSubnet(_lanSubnet, info).has_value();
}

bool SubnetManager::IsWan(const SockAddr_in& info)
{
    return FindSubnet(_wanSubnet, info).has_value();
}

std::optional<SubnetManager::SubnetInfo> SubnetManager::FindSubnet(Vector<SubnetInfo>& group, const SockAddr_in info)
{
    const auto result = std::ranges::find_if(group
        , [info](const SubnetInfo& data)
        {
            return Socket::IsSameSubnet(data.address, info.sin_addr, data.subnetSize);
        });

    if (result == group.end())
    {
        return {};
    }
    return *result;
}

void SubnetManager::LoadSubnets()
{
    _subnetParser.Init(L"subnetSetting.txt");
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

        container.push_back({subnet, Socket::Ip2Address(ip.c_str()), ip});
    }

    std::ranges::sort(container, [](const SubnetInfo& d1, const SubnetInfo& d2)
    {
        return d1.subnetSize > d2.subnetSize;
    });
}
