#pragma once

#include <functional>
#include <queue>
#include <memory>
#include <vector>
#include <chrono>
#include <mutex>

#include <brynet/net/Noexcept.h>

namespace brynet { namespace timer {

    class TimerMgr;

    class Timer final
    {
    public:
        using Ptr = std::shared_ptr<Timer>;
        using WeakPtr = std::weak_ptr<Timer>;
        using Callback = std::function<void(void)>;

        Timer(std::chrono::steady_clock::time_point startTime, std::chrono::nanoseconds lastTime, Callback&& f) BRYNET_NOEXCEPT;

        const std::chrono::steady_clock::time_point&    getStartTime() const;
        const std::chrono::nanoseconds&                 getLastTime() const;

        std::chrono::nanoseconds                        getLeftTime() const;
        void                                            cancel();

    private:
        void operator()                                 ();

    private:
        std::once_flag                                  mExecuteOnceFlag;
        Callback                                        mCallback;
        const std::chrono::steady_clock::time_point     mStartTime;
        std::chrono::nanoseconds                        mLastTime;

        friend class TimerMgr;
    };

    class TimerMgr final
    {
    public:
        using Ptr = std::shared_ptr<TimerMgr>;

        template<typename F, typename ...TArgs>
        Timer::WeakPtr                          addTimer(std::chrono::nanoseconds timeout, F&& callback, TArgs&& ...args)
        {
            auto timer = std::make_shared<Timer>(
                std::chrono::steady_clock::now(),
                std::chrono::nanoseconds(timeout),
                std::bind(std::forward<F>(callback), std::forward<TArgs>(args)...));
            mTimers.push(timer);

            return timer;
        }

        void                                    addTimer(const Timer::Ptr& timer)
        {
            mTimers.push(timer);
        }

        void                                    schedule();
        bool                                    isEmpty() const;
        // if timer empty, return zero
        std::chrono::nanoseconds                nearLeftTime() const;
        void                                    clear();

    private:
        class CompareTimer
        {
        public:
            bool operator() (const Timer::Ptr& left, const Timer::Ptr& right) const
            {
                const auto startDiff = left->getStartTime() - right->getStartTime();
                const auto lastDiff = left->getLastTime() - right->getLastTime();
                const auto diff = startDiff.count() + lastDiff.count();
                return diff > 0;
            }
        };

        std::priority_queue<Timer::Ptr, std::vector<Timer::Ptr>, CompareTimer>  mTimers;
    };

} }