#pragma once

#include <functional>
#include <queue>
#include <memory>
#include <vector>
#include <chrono>

#include <brynet/net/Noexcept.h>

namespace brynet { namespace timer {

    using namespace std::chrono;
    class TimerMgr;

    class Timer
    {
    public:
        typedef std::shared_ptr<Timer>          Ptr;
        typedef std::weak_ptr<Timer>            WeakPtr;
        typedef std::function<void(void)>       Callback;

        Timer(steady_clock::time_point startTime, nanoseconds lastTime, Callback f) BRYNET_NOEXCEPT;

        const steady_clock::time_point&         getStartTime() const;
        const nanoseconds&                      getLastTime() const;

        nanoseconds                             getLeftTime() const;
        void                                    cancel();

    private:
        void operator()                         ();

    private:
        bool                                    mActive;
        Callback                                mCallback;
        const steady_clock::time_point          mStartTime;
        nanoseconds                             mLastTime;

        friend class TimerMgr;
    };

    class TimerMgr
    {
    public:
        typedef std::shared_ptr<TimerMgr>   PTR;

        template<typename F, typename ...TArgs>
        Timer::WeakPtr                          addTimer(nanoseconds timeout, F callback, TArgs&& ...args)
        {
            auto timer = std::make_shared<Timer>(
                steady_clock::now(), 
                nanoseconds(timeout),
                std::bind(std::move(callback), std::forward<TArgs>(args)...));
            mTimers.push(timer);

            return timer;
        }

        void                                    schedule();
        bool                                    isEmpty() const;
        // if timer empty, return zero
        nanoseconds                             nearLeftTime() const;
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