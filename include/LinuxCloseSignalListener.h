#pragma once

#include <pthread.h>
#include <signal.h>

#include <exception>
#include <functional>
#include <system_error>
#include <thread>
#include <utility>

#include <cerrno>

namespace psh::util
{
    class LinuxCloseSignalListener
    {
    public:
        explicit LinuxCloseSignalListener(std::function<void()> func)
        {
            sigset_t mask;
            sigemptyset(&mask);
            sigaddset(&mask, SIGTERM);
            sigaddset(&mask, SIGINT);

            //  시그널 마스크 설정 실패
            if (pthread_sigmask(SIG_BLOCK, &mask, &previousMask_) != 0)
                throw std::runtime_error("Failed to set signals");

            try
            {
                thread_ = std::jthread(
                    [mask, func = std::move(func)](std::stop_token token)
                    {
                        while (!token.stop_requested())
                        {
                            timespec timeout{0, 100'000'000};
                            auto signal = sigtimedwait(&mask, nullptr, &timeout);

                            if (signal > 0 && !token.stop_requested())
                            {
                                func();
                                continue;
                            }

                            //  정상: 타임아웃 또는 다른 핸들러에서 처리할 시그널이 켜짐
                            const int error = errno;
                            if (error == EAGAIN || error == EINTR)
                                continue;

                            //  비정상적인 경우
                            return;
                        }
                    });
            }
            catch (...)
            {
                //  스레드 생성 실패 시 롤백
                pthread_sigmask(SIG_SETMASK, &previousMask_, nullptr);
                throw;
            }
        }

        ~LinuxCloseSignalListener()
        {
            thread_.request_stop();
            thread_.join();

            pthread_sigmask(SIG_SETMASK, &previousMask_, nullptr);
        }

        LinuxCloseSignalListener(const LinuxCloseSignalListener&) = delete;
        LinuxCloseSignalListener& operator=(const LinuxCloseSignalListener&) = delete;

    private:
        sigset_t previousMask_{};
        std::jthread thread_;
    };
}
