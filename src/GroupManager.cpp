#include "GroupManager.h"
#include "GroupExecutable.h"
#include "IOCP.h"


void GroupManager::MoveSession(const SessionID target, const GroupID dst, bool update)
{
    const auto session = _owner->FindSession(target, L"MoveSession");

    if (session == nullptr)
    {
        return;
    }
    GroupID src = session->GetGroupID();


    {
        std::shared_lock lock(_groupLock);

        if (src != BaseGroupID())
        {
            const auto& srcGroup = _groups.find(src)->second;
#ifdef SESSION_DEBUG
		    session->Write(-1, src, L"MoveLeave");
#endif
            srcGroup->LeaveSession(session->GetSessionId(), update, session->GetErr());
        }
        session->SetGroupID(dst);
        if (dst != BaseGroupID())
        {
            const auto& dstGroup = _groups.find(dst)->second;
#ifdef SESSION_DEBUG
		    session->Write(-1, dst, L"MoveEnter");
#endif
            dstGroup->EnterSession(session->GetSessionId(), update);
        }
    }
    session->Release(L"MoveSessionRel");
}


void GroupManager::Update()
{
    std::shared_lock lock(_groupLock);
    for (auto& [groupId, groupPtr] : _groups)
    {
        if (groupPtr->NeedUpdate())
        {
            groupPtr->_executable->Clear();
            _owner->PostExecutable(groupPtr->_executable.get(), 0);
        }
    }
}

bool GroupManager::OnRecv(SessionID sessionId, GroupID groupId)
{
    //객체의 주소를 받아오고 나서 락을 해제해도 문제 x
    //해쉬맵에 데이터가 추가되면서 객체가 이동할 수 있으나 uniqueptr이 가리키는 원본 포인터는 변하지 않을 것.
    Group* ptr;
    {
        std::shared_lock lock(_groupLock);
        ptr = _groups[groupId].get();
    }
    return ptr->Enqueue({Group::GroupJob::Recv, sessionId});
}

void GroupManager::SessionDisconnected(Session* target)
{
    GroupID src = target->GetGroupID();

    std::shared_lock lock(_groupLock);

    if (src != BaseGroupID())
    {
        auto& srcGroup = _groups.find(src)->second;
#ifdef SESSION_DEBUG
		target->Write(-1, src, L"Disconnect Leave");
#endif
        srcGroup->LeaveSession(target->GetSessionId(), true, target->GetErr());
    }

}


GroupManager::GroupManager(IOCP* owner)
    : _owner(owner)
{
}
