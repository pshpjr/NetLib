#include "RecvExecutable.h"
#include "Logger.h"
#include "CoreGlobal.h"
#include "RecvBuffer.h"
#include "IOCP.h"
#include "Protocol.h"
#include "Session.h"


void RecvExecutable::Execute(const ULONG_PTR key, const DWORD transferred, void* iocp)
{
    Session& session = *reinterpret_cast<Session*>(key);

    session._recvQ.MoveRear(transferred);

    IOCP& server = *reinterpret_cast<IOCP*>(iocp);
    if (session.GetGroupID() != GroupManager::BaseGroupID())
    {
        server._groupManager->OnRecv(session.GetSessionId(), session.GetGroupID());
        return;
    }
    if (session._staticKey)
    {
        RecvHandler<NetHeader>(session, iocp);
    }
    else
    {
        RecvHandler<LANHeader>(session, iocp);
    }

    session.RecvNotIncrease();
}

template <typename Header>
void RecvExecutable::RecvHandler(Session& session, void* iocp)
{
    IOCP& server = *static_cast<IOCP*>(iocp);
    int loopCount = 0;

    RingBuffer& recvQ = session._recvQ;
    while (true)
    {
        if (session.GetGroupID() != GroupManager::BaseGroupID())
        {
            break;
        }

        if (recvQ.Size() < sizeof(Header))
        {
            break;
        }


        Header header;
        recvQ.Peek(reinterpret_cast<char*>(&header), sizeof(Header));

        if constexpr (std::is_same_v<Header, NetHeader>)
        {
            if (header.code != dfPACKET_CODE)
            {
                int frontIndex = recvQ.GetFrontIndex();
                gLogger->Write(L"Disconnect", Logger::LogLevel::Debug, L"WrongPacketCode id : %d\n index: %d \n"
                    , session.GetSessionId().id, frontIndex);
                session.Close();
                break;
            }

            if (header.len > session._maxPacketLen)
            {
                int frontIndex = recvQ.GetFrontIndex();
                gLogger->Write(L"Disconnect", Logger::LogLevel::Debug, L"WrongPacketLen id : %d\n index: %d \n"
                    , session.GetSessionId().id, frontIndex);
                session.Close();
                break;
            }
        }

        if (header.len > recvQ.Size())
        {
            break;
        }


        if (const int totPacketSize = header.len + sizeof(Header);
            recvQ.Size() < totPacketSize)
        {
            break;
        }

        RecvBuffer buffer(&recvQ, header.len);
        //auto& buffer = *RecvBuffer::Alloc(&recvQ,header.len);

        if constexpr (std::is_same_v<Header, NetHeader>)
        {
            recvQ.Decode(session._staticKey);

            if (!recvQ.ChecksumValid())
            {
                gLogger->Write(L"Disconnect", Logger::LogLevel::Debug, L"Recv Invalid Checksum id : %d"
                    , session.GetSessionId().id);
                session.Close();
                break;
            }
        }
        loopCount++;
        recvQ.Dequeue(sizeof(Header));
        server.onRecvPacket(session, buffer);

        ASSERT_CRASH(buffer.CanPopSize() == 0, "UnuseData");
    }

    if (loopCount > 0)
    {
        server.IncreaseRecvCount(loopCount);
    }
}
