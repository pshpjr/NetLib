#include "PostSendExecutable.h"
#include "SendBuffer.h"
#include "IOCP.h"
#include "Session.h"
#include "stdafx.h"

void PostSendExecutable::Execute(const ULONG_PTR key, DWORD transferred, void* iocp)
{
    UNREFERENCED_PARAMETER(transferred);
    IOCP& server = *static_cast<IOCP*>(iocp);
    server.increaseSendCount(dataNotSend);


    const auto session = reinterpret_cast<Session*>(key);

    //TODO: sendingQ 세션에 있을 필요 없음. 
    const auto& sendingQ = session->_sendingQ;

    for (int i = 0; i < dataNotSend; i++)
    {
        sendingQ[i]->Release(L"PostSendRelease");
    }
    dataNotSend = 0;


    session->_needCheckSendTimeout = false;


    session->_isSending = 0;

    session->TrySend();

    session->Release(L"PostSendRelease");
}
