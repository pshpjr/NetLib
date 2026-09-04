#include "ReleaseExecutable.h"
#include "IOCP.h"
#include "Session.h"
#include "stdafx.h"

void ReleaseExecutable::Execute(const ULONG_PTR key, DWORD transferred, void* iocp)
{
    UNREFERENCED_PARAMETER(transferred);
    UNREFERENCED_PARAMETER(iocp);
    const auto session = reinterpret_cast<Session*>(key);
    //--session->_owner->_iocpCompBufferSize;


    session->_owner->_onDisconnect(session->GetSessionId(), session->GetErr());

    session->Reset();
    const unsigned short index = session->GetSessionId().index;
    session->_owner->ReleaseFreeIndex(index);
}
