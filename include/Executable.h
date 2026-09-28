#pragma once
#include <array>
#include <functional>
#include <string>
#include <type_traits>
#include <utility>

#include <WinSock2.h>
#include <Windows.h>
#include "SocketTypes.h"

class Executable final
{
public:
    using Callback = std::function<void(NetError, DWORD)>;

    enum class IoType
    {
        Base = 0,
        Send,
        Recv,
        Postrecv,
        Release,
        Group,
        Custom
    };

    explicit Executable(IoType type, Callback callback = {})
        : _type{type}, _overlapped(this), _callback(std::move(callback))
    {
    }

    Executable(const Executable&) = delete;
    Executable(Executable&&) = delete;
    Executable& operator=(const Executable&) = delete;
    Executable& operator=(Executable&&) = delete;

    void SetCallback(Callback callback)
    {
        _callback = std::move(callback);
    }

    void Execute(NetError error, DWORD transferred)
    {
        _callback(error, transferred);
    }

    void Clear()
    {
        _overlapped.Clear();
    }

    LPOVERLAPPED GetOverlapped()
    {
        return _overlapped.Get();
    }

    static Executable* GetExecutable(OVERLAPPED* overlap)
    {
        return overlap ? reinterpret_cast<OverlappedIO*>(overlap)->Owner() : nullptr;
    }

    static const std::wstring& GetIoType(const Executable& executable)
    {
        return ioTypeToStr[static_cast<char>(executable._type)];
    }
private:

    class OverlappedIO
    {
    public:
        explicit OverlappedIO(Executable* owner) : _owner(owner)
        {
            Clear();
        }

        void Clear()
        {
            memset(&_overlapped, 0, sizeof(_overlapped));
        }

        LPOVERLAPPED Get()
        {
            return &_overlapped;
        }

        Executable* Owner() const
        {
            return _owner;
        }

    private:
        OVERLAPPED _overlapped;
        Executable* _owner;
    };

    static_assert(std::is_standard_layout_v<OverlappedIO>);

    inline static std::array ioTypeToStr = 
    {
        std::wstring(L"BASE"),
        std::wstring(L"SEND"),
        std::wstring(L"RECV"),
        std::wstring(L"POST_RECV"),
        std::wstring(L"RELEASE"),
        std::wstring(L"GROUP"),
        std::wstring(L"CUSTOM")
    };

    IoType _type{IoType::Base};
    OverlappedIO _overlapped;
    Callback _callback;
};
