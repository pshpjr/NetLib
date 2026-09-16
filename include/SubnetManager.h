#pragma once
#include "SettingParser.h"
#include "SocketTypes.h"

#include <optional>
#include <vector>

class SubnetManager
{
    // netlib-order: prerequisite - public return type needs a private forward declaration.
    struct SubnetInfo;

public:
    SubnetManager();

    bool IsLan(const SockAddr_in& info);

    bool IsWan(const SockAddr_in& info);

    std::optional<SubnetInfo> FindMatchSubnet(const SockAddr_in info)
    {
        if (auto subnetResult = FindSubnet(_lanSubnet, info);
            subnetResult.has_value())
        {
            return subnetResult;
        }
        return FindSubnet(_wanSubnet, info);
    }
private:
    struct SubnetInfo
    {
        char subnetSize;
        IN_ADDR address;
        String addressStr;
    };
    std::optional<SubnetInfo> FindSubnet(Vector<SubnetInfo>& group, SockAddr_in info);

    void LoadSubnets();
    void LoadSubnetGroup(String group, Vector<SubnetInfo>& container);

    SettingParser _subnetParser;
    Vector<SubnetInfo> _lanSubnet;
    Vector<SubnetInfo> _wanSubnet;
};
