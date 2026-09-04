//
// Created by pshpj on 24. 9. 2.
//

#ifndef GROUPTYPES_H
#define GROUPTYPES_H
#include <atomic>
#include "Types.h"

class GroupID
{
public:
    explicit constexpr GroupID(const int id) : _id(id)
    {
    }
    static GroupID NextID()
    {
        return GroupID(++g_GroupID);
    }

    static consteval GroupID InvalidGroupID()
    {
        return GroupID{-1};
    }

    explicit operator long() const
    {
        return _id;
    }

    friend bool operator==(const GroupID& lhs, const GroupID& rhs)
    {
        return lhs._id == rhs._id;
    }

    friend bool operator!=(const GroupID& lhs, const GroupID& rhs)
    {
        return !(lhs == rhs);
    }

    friend std::hash<GroupID>;
    friend std::equal_to<GroupID>;

private:
    long _id;
    inline static std::atomic<long> g_GroupID = 0;
};



namespace std
{
    template <>
    struct hash<GroupID>
    {
    public:
        size_t operator()(const GroupID& id) const noexcept
        {
            return std::hash<long>()(id._id);
        }
    };

}
#endif //GROUPTYPES_H
