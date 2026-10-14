#pragma once

#include "MyWindows.h"
#include "ExceptionTrace.h"

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace psh::util
{
    // 프로세스당 하나만 등록할 수 있다. 콜백은 Windows 핸들러 스레드에서 실행된다.
    // 콜백 안에서 리스너를 생성/파괴하지 않고, 캡처한 객체는 리스너보다 오래 유지한다.
    class WindowsCloseSignalListener
    {
    public:
        explicit WindowsCloseSignalListener(std::function<void()> func)
        {
            if (!func)
                throw std::invalid_argument("Shutdown callback must not be empty");

            auto state = std::make_shared<State>(std::move(func));
            std::lock_guard lock(mutex_);
            if (active_)
                throw std::logic_error("A shutdown listener is already registered");

            if (!SetConsoleCtrlHandler(&HandleSignal, TRUE))
                throw std::system_error(static_cast<int>(GetLastError()),
                                        std::system_category(),
                                        "Failed to register console control handler");

            active_ = std::move(state);
        }

        ~WindowsCloseSignalListener()
        {
            // 콜백이 끝난 뒤 등록을 해제한다. 이미 시작된 핸들러도 active_를 확인한다.
            std::lock_guard lock(mutex_);
            SetConsoleCtrlHandler(&HandleSignal, FALSE);
            active_->completed = true;
            active_.reset();
            completed_.notify_all();
        }

        WindowsCloseSignalListener(const WindowsCloseSignalListener&) = delete;
        WindowsCloseSignalListener& operator=(const WindowsCloseSignalListener&) = delete;
        WindowsCloseSignalListener(WindowsCloseSignalListener&&) = delete;
        WindowsCloseSignalListener& operator=(WindowsCloseSignalListener&&) = delete;

    private:
        struct State
        {
            explicit State(std::function<void()> func) : callback(std::move(func)) {}

            std::function<void()> callback;
            bool completed = false;
        };

        static ::BOOL WINAPI HandleSignal(::DWORD signal) noexcept
        {
            switch (signal)
            {
            case CTRL_C_EVENT:
            case CTRL_BREAK_EVENT:
            case CTRL_CLOSE_EVENT:
            case CTRL_LOGOFF_EVENT:
            case CTRL_SHUTDOWN_EVENT:
                break;
            default:
                return FALSE;
            }

            try
            {
                std::unique_lock lock(mutex_);
                auto state = active_;
                if (!state)
                    return FALSE;

                RunWithTrace("Shutdown listener thread", [&] { state->callback(); });

                // 이 신호들은 핸들러가 반환하면 프로세스가 종료된다.
                // 소유 스레드가 정리를 마치고 리스너를 파괴할 때까지 대기한다.
                // Windows의 종료 제한 시간이 지나면 OS가 강제로 종료할 수 있다.
                if (signal != CTRL_C_EVENT && signal != CTRL_BREAK_EVENT)
                    completed_.wait(lock, [&] { return state->completed; });

                return TRUE;
            }
            catch (...)
            {
                // 예외를 Win32 콜백 경계 밖으로 내보내지 않는다.
                return FALSE;
            }
        }

        inline static std::mutex mutex_;
        inline static std::condition_variable completed_;
        inline static std::shared_ptr<State> active_;
    };
}
