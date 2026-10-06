#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

#ifdef _WIN32
#include "MyWindows.h"
#endif

#include "SocketTypes.h"

#ifdef _WIN32
class WindowsExecutable final
{
public:
    using Callback = std::function<void(NetError, std::size_t)>;

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

    explicit WindowsExecutable(IoType type, Callback callback = {})
        : _type{type}, _overlapped(this), _callback(std::move(callback))
    {
    }

    WindowsExecutable(const WindowsExecutable&) = delete;
    WindowsExecutable(WindowsExecutable&&) = delete;
    WindowsExecutable& operator=(const WindowsExecutable&) = delete;
    WindowsExecutable& operator=(WindowsExecutable&&) = delete;

    void SetCallback(Callback callback)
    {
        _callback = std::move(callback);
    }

    void SetResult(NetError error, std::size_t transferred)
    {
        assert(!_result.has_value());
        _result.emplace(error, transferred);
    }

    bool HasResult() const noexcept
    {
        return _result.has_value();
    }

    void Execute()
    {
        assert(_result.has_value());
        const auto [error, transferred] = _result.value();
        // The callback may register the next result on this executable.
        _result.reset();
        _callback(error, transferred);
    }

    void Clear()
    {
        _overlapped.Clear();
        _result.reset();
    }

    LPOVERLAPPED GetOverlapped()
    {
        return _overlapped.Get();
    }

    IoType GetIoType()
    {
        return _type;
    }

    const std::wstring& GetIoTypeStr()
    {
        return ioTypeToStr[static_cast<char>(_type)];
    }

    static WindowsExecutable* GetExecutable(OVERLAPPED* overlap)
    {
        return overlap ? reinterpret_cast<OverlappedIO*>(overlap)->Owner() : nullptr;
    }
private:

    class OverlappedIO
    {
    public:
        explicit OverlappedIO(WindowsExecutable* owner) : _owner(owner)
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

        WindowsExecutable* Owner() const
        {
            return _owner;
        }

    private:
        OVERLAPPED _overlapped;
        WindowsExecutable* _owner;
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
    std::optional<std::tuple<NetError, std::size_t>> _result;
};

using Executable = WindowsExecutable;
#else
class LinuxExecutable final
{
public:
    using Callback = std::function<void(NetError, std::size_t)>;

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

    explicit LinuxExecutable(IoType type, Callback callback = {})
        : _type{type}, _callback(std::move(callback))
    {
    }

    LinuxExecutable(const LinuxExecutable&) = delete;
    LinuxExecutable(LinuxExecutable&&) = delete;
    LinuxExecutable& operator=(const LinuxExecutable&) = delete;
    LinuxExecutable& operator=(LinuxExecutable&&) = delete;

    void SetCallback(Callback callback)
    {
        _callback = std::move(callback);
    }

    void SetResult(NetError error, std::size_t transferred)
    {
        assert(!_result.has_value());
        _result.emplace(error, transferred);
    }

    bool HasResult() const noexcept
    {
        return _result.has_value();
    }

    void Execute()
    {
        assert(_result.has_value());
        const auto [error, transferred] = _result.value();
        // The callback may register the next result on this executable.
        _result.reset();
        _callback(error, transferred);
    }

    void Clear()
    {
        _result.reset();
    }

    IoType GetIoType()
    {
        return _type;
    }

    const std::wstring& GetIoTypeStr()
    {
        return ioTypeToStr[static_cast<char>(_type)];
    }

private:
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
    Callback _callback;
    std::optional<std::tuple<NetError, std::size_t>> _result;
};

using Executable = LinuxExecutable;
#endif
