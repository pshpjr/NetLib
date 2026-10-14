#pragma once
#include "GroupTypes.h"
#include "Macro.h"
#include "Types.h"

#include <optional>

namespace psh
{
    using AtomicRecvState = std::atomic<uint64>;

    struct RecvState
    {
        enum Flag : uint32
        {
            None            = 0,
            Moving          = 1 << 0,       // 그룹 이동 중
            RecvScheduled   = 1 << 1,       // 세션 처리를 스케줄링 됐거나 실행 중
            RecvDirty       = 1 << 2,       // 버퍼를 다시 확인해야 함. 데이터가 추가되었을 수 있음.
            RecvPaused      = 1 << 3,       // 버퍼가 가득 차 recv를 걸지 않음
        };

        uint64 data = 0;

        constexpr RecvState() = default;

        constexpr RecvState(uint64 value)
            : data(value)
        {
        }

        RecvState(GroupID group, uint32 flags)
            : data(PackGroupId(group) | flags)
        {
        }

        constexpr operator uint64() const
        {
            return data;
        }

        GroupID GetGroupID() const
        {
            return GroupID(static_cast<int32>(data >> 32));
        }

        bool Has(uint32 flags) const
        {
            return (data & flags) != 0;
        }

        RecvState With(uint32 flags) const
        {
            return data | flags;
        }

        RecvState Without(uint32 flags) const
        {
            return data & ~static_cast<uint64>(flags);
        }

        RecvState WithGroupId(GroupID id) const
        {
            return PackGroupId(id) | (data & 0xFFFF'FFFFull);
        }

        static uint64 PackGroupId(GroupID id)
        {
            return static_cast<uint64>(static_cast<uint32>(static_cast<long>(id))) << 32;
        }

        ////////////////////
        // 상태 전이
        ////////////////////
        
        //  리턴 값이 있다 = 소비 권한을 얻었다. 
        //  대상 그룹에게 전달해 줘야 한다. 
        //  TryBeginMove 제외하고는 내가 스케줄링 할 그룹에 넣어야 하는 경우 그룹 ID 리턴

        static std::optional<GroupID> MarkRecvDirty(AtomicRecvState& state)
        {
            return MarkAndTrySchedule(state, RecvDirty);
        }

        static std::optional<GroupID> MarkPauseRecv(AtomicRecvState& state)
        {
            ASSERT_CRASH(!RecvState(state).Has(RecvPaused), L"recv paused twice");
            return MarkAndTrySchedule(state, RecvDirty | RecvPaused);
        }


        // 이동 중이면 nullopt. 성공하면 이동 전 그룹 리턴
        static std::optional<GroupID> TryBeginMove(AtomicRecvState& state, GroupID dst)
        {
            RecvState seen = state.load();
            for (;;)
            {
                if (seen.Has(Moving))
                    return std::nullopt;

                if (state.compare_exchange_weak(seen.data, seen.With(Moving | RecvDirty).WithGroupId(dst)))
                {
                    return seen.GetGroupID();
                }

            }
        }
        
        static std::optional<GroupID> CompleteMove(AtomicRecvState& state)
        {
            RecvState seen = state.load();
            for (;;)
            {
                ASSERT_CRASH(seen.Has(Moving), L"CompleteMove without Moving");

                const bool schedule = !seen.Has(RecvScheduled);
                const auto next = seen.Without(Moving).With(schedule ? RecvScheduled : None);
                
                if (state.compare_exchange_weak(seen.data, next))
                {
                    return schedule ? std::optional(seen.GetGroupID()) : std::nullopt;
                }
 
            }
        }

    private:
        static bool CanSchedule(const RecvState& state)
        {
            return !state.Has(RecvScheduled | Moving);
        }

        static std::optional<GroupID> MarkAndTrySchedule(AtomicRecvState& state, uint32 flags)
        {
            RecvState seen = state.load();
            for (;;)
            {
                const bool canSchedule = CanSchedule(seen);
                auto next = seen.With(flags | (canSchedule ? RecvScheduled : None));
                
                if (state.compare_exchange_weak(seen.data, next))
                {
                    return canSchedule ? std::optional(seen.GetGroupID()) : std::nullopt;
                }

            }
        }
    };

}