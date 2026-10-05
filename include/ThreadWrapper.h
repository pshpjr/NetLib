#pragma once
#include <thread>


namespace psh
{
    namespace thread::detail
    {
        inline thread_local bool isLibThread = false;
    }

    class ThreadWrapper
    {

    public:

        bool IsLibThread() const { return thread::detail::isLibThread; }

        template <typename F, typename... Args>
        explicit ThreadWrapper(F&& f, Args&&... args)
            : thread_([this, func = std::forward<F>(f)]
                      <typename... InnerArgs>
                      (InnerArgs&&... innerArgs) mutable
                      {
                          thread::detail::isLibThread = true;

                          std::invoke(func, std::forward<InnerArgs>(innerArgs)...);

                          thread::detail::isLibThread = false;
                      },
                  std::forward<Args>(args)...)
        {
        }

        bool joinable() const
        {
            return thread_.joinable();
        }

        void join()
        {
            return thread_.join();
        }

    private:
        std::thread thread_;
    };




}