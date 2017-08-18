#ifndef BRYNET_TIMER_H_
#define BRYNET_TIMER_H_

#include <functional>
#include <queue>
#include <memory>
#include <vector>
#include <chrono>

#include "systemlib.h"

namespace brynet
{
    class TimerMgr;

    class Timer
    {
    public:
        typedef std::shared_ptr<Timer>          Ptr;
        typedef std::weak_ptr<Timer>            WeakPtr;
        typedef std::function<void(void)>       Callback;

        Timer(std::chrono::steady_clock::time_point endTime, Callback f) noexcept;

        const std::chrono::steady_clock::time_point&    getEndMs() const;
        void                                    cancel();

    private:
        void operator()                         ();

    private:
        bool                                    mActive;
        Callback                                mCallback;
        const std::chrono::steady_clock::time_point mEndTime;

        friend class TimerMgr;
    };

    class TimerMgr
    {
    public:
        typedef std::shared_ptr<TimerMgr>   PTR;

        template<typename F, typename ...TArgs>
        Timer::WeakPtr                          addTimer(time_t delayMs, F callback, TArgs&& ...args)
        {
            auto timer = std::make_shared<Timer>(std::chrono::steady_clock::now() + std::chrono::microseconds(delayMs),
                                                std::bind(callback, std::forward<TArgs>(args)...));
            mTimers.push(timer);

            return timer;
        }

        void                                    schedule();
        bool                                    isEmpty() const;
        std::chrono::milliseconds               nearEndMs() const;
        void                                    clear();

    private:
        class CompareTimer
        {
        public:
            bool operator() (const Timer::Ptr& left, const Timer::Ptr& right) const
            {
                return left->getEndMs() > right->getEndMs();
            }
        };

        std::priority_queue<Timer::Ptr, std::vector<Timer::Ptr>, CompareTimer>  mTimers;
    };
}

#endif