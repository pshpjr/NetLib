#pragma once
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <type_traits>
#include "Group.h"

class GroupManager
{
    friend class Group;

public:
    GroupManager(IOCP* owner);

    /*그룹 삭제 고려 없이 작성했음. 그룹은 풀링 형식으로 관리하고, reset/stop  형태로 관리하게 고려함. */
    template <typename GroupType, typename... Args>
    GroupID CreateGroup(Args&&... args);

    //move 0이면 나가기. 
    void MoveSession(SessionID target, GroupID dst, bool update = true);
    void SessionDisconnected(Session* target);

    void Update();

    bool OnRecv(SessionID sessionId, GroupID groupId);

    static consteval GroupID BaseGroupID() {return GroupID(0);} 
private:
    std::shared_mutex _groupLock;
    HashMap<GroupID, std::unique_ptr<Group>> _groups;

    IOCP* _owner;

    /// <summary>
    /// 0번 그룹은 그룹이 없는 것을 의미한다. 
    /// </summary>
};


template <typename GroupType, typename... Args>
GroupID GroupManager::CreateGroup(Args&&... args)
{
    static_assert(std::is_base_of_v<Group, GroupType>, "GroupType must inherit Group");

    Group* newGroup = new GroupType(std::forward<Args>(args)...);
    newGroup->_groupId = GroupID::NextID();
    newGroup->_iocp = _owner;
    newGroup->_owner = this;

    newGroup->OnCreate();
    {
        std::unique_lock lock(_groupLock);
        _groups.emplace(newGroup->_groupId, std::unique_ptr<Group>(newGroup));
    }

    return newGroup->_groupId;
}
