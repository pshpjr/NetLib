#include "GroupManager.h"
#include "Executable.h"
#include "IOCP.h"


void GroupManager::MoveSession(const SessionID target, const GroupID dst)
{
    const auto session = _owner->FindSession(target, L"MoveSession");

    if (session == nullptr)
        return;

    {
        std::shared_lock groupLock(_groupLock);
        if (auto it = _groups.find(dst); it == _groups.end())
            return;
    }


    const auto prevGroup = psh::RecvState::TryBeginMove(session->_state, dst);
    if (!prevGroup)
    {
        // 이전 이동의 OnEnter가 아직 안 끝났다. 이동이 중첩될 경우 처리가 안 되므로, 나중에 다시 시도한다.
        if (!_owner->_executor.Post(
                [this, target, dst, session]
                {
                    MoveSession(target, dst);
                    session->Release(L"DeferredMove");
                }))
        {
            session->Release(L"DeferredMoveRejected");
        }
        return;
    }

    std::shared_lock groupLock(_groupLock);

    if (*prevGroup != BaseGroupID())
    {
        _groups.find(*prevGroup)->second->LeaveSession(target, session->GetErr());
    }

    if (dst != BaseGroupID())
    {
        _groups.find(dst)->second->EnterSession(target);
    }

    groupLock.unlock();

    // Base는 Enter job이 없으므로 여기서 바로 이동을 끝낸다.
    if (dst == BaseGroupID())
        session->CompleteMove();

    session->Release(L"MoveSessionRel");
}


void GroupManager::Update()
{
    std::shared_lock groupLock(_groupLock);
    for (auto& [groupId, groupPtr] : _groups)
    {
        if (groupPtr->NeedUpdate())
        {
            groupPtr->RequestUpdate();
        }
    }
    groupLock.unlock();
}

void GroupManager::OnRecv(SessionID sessionId, GroupID groupId, std::size_t transferred)
{
    //객체의 주소를 받아오고 나서 락을 해제해도 문제 x
    //해쉬맵에 데이터가 추가되면서 객체가 이동할 수 있으나 uniqueptr이 가리키는 원본 포인터는 변하지 않을 것.
    std::shared_lock groupLock(_groupLock);
    auto ptr = _groups[groupId].get();
    groupLock.unlock();
    ptr->Enqueue({Group::GroupJob::Recv, sessionId, 0, GroupID(0), transferred});
}

void GroupManager::SessionDisconnected(Session* target)
{
    GroupID src = target->GetGroupID();

    std::shared_lock groupLock(_groupLock);

    if (src != BaseGroupID())
    {
        auto& srcGroup = _groups.find(src)->second;
#ifdef SESSION_DEBUG
		target->Write(-1, src, L"Disconnect Leave");
#endif
        srcGroup->LeaveSession(target->GetSessionId(), target->GetErr());
    }

    groupLock.unlock();
}


GroupManager::GroupManager(IOCP* owner)
    : _owner(owner)
{
    
}
