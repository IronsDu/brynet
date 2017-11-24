#ifndef BRYNET_TIMER_H_
#define BRYNET_TIMER_H_

#include <functional>
#include <queue>
#include <memory>
#include <vector>
#include <chrono>

#include <brynet/net/Noexcept.h>

namespace brynet
{
    class TimerMgr;

    class Timer
    {
    public:
        typedef std::shared_ptr<Timer>          Ptr;
        typedef std::weak_ptr<Timer>            WeakPtr;
        typedef std::function<void(void)>       Callback;

        Timer(std::chrono::steady_clock::time_point startTime, 
            std::chrono::nanoseconds lastTime, 
            Callback f) BRYNET_NOEXCEPT;

        const std::chrono::steady_clock::time_point&    getStartTime() const;
        const std::chrono::nanoseconds&         getLastTime() const;
        void                                    cancel();

    private:
        void operator()                         ();

    private:
        bool                                    mActive;
        Callback                                mCallback;
        const std::chrono::steady_clock::time_point mStartTime;
        std::chrono::nanoseconds                mLastTime;

        friend class TimerMgr;
    };

    class TimerMgr
    {
    public:
        typedef std::shared_ptr<TimerMgr>   PTR;

        template<typename F, typename ...TArgs>
        Timer::WeakPtr                          addTimer(std::chrono::nanoseconds timeout, 
                                                         F callback, 
                                                         TArgs&& ...args)
        {
            auto timer = std::make_shared<Timer>(std::chrono::steady_clock::now(),
                                                std::chrono::nanoseconds(timeout),
                                                std::bind(std::move(callback), std::forward<TArgs>(args)...));
            mTimers.push(timer);

            return timer;
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
                auto startDiff = (left->getStartTime() - right->getStartTime());
                auto lastDiff = left->getLastTime() - right->getLastTime();
                auto diff = startDiff.count() + lastDiff.count();
                return diff > 0;
            }
        };

        std::priority_queue<Timer::Ptr, std::vector<Timer::Ptr>, CompareTimer>  mTimers;
    };
}

#endif