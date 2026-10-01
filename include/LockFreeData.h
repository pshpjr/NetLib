#pragma once
namespace lock_free_data
{
    constexpr unsigned long long pointerMask = 0x000'7FFF'FFFF'FFFF;
    constexpr unsigned long long indexInc = 0x000'8000'0000'0000;
    constexpr unsigned long long indexMask = 0xFFFF'8000'0000'0000;
}
