#ifndef DODO_TIMER_H_
#define DODO_TIMER_H_

#include <functional>
#include <queue>
#include <memory>
#include <vector>

#include "systemlib.h"

namespace dodo
{
    class Timer
    {
    public:
        typedef std::shared_ptr<Timer>          Ptr;
        typedef std::weak_ptr<Timer>            WeakPtr;
        typedef std::function<void(void)>       Callback;

        Timer(time_t endTime, Callback f);

        time_t                                  getEndMs() const;
        void                                    cancel();
        void operator()                         ();

    private:
        bool                                    mActive;
        Callback                                mCallback;
        const time_t                            mEndTime;
    };

    class TimerMgr
    {
    public:
        typedef std::shared_ptr<TimerMgr>   PTR;

        template<typename F, typename ...TArgs>
        Timer::WeakPtr                          addTimer(time_t delayMs, F callback, TArgs&& ...args)
        {
            auto timer = std::make_shared<Timer>(delayMs + static_cast<time_t>(ox_getnowtime()),
                                                std::bind(callback, std::forward<TArgs>(args)...));
            mTimers.push(timer);

            return timer;
        }

        void                                    schedule();
        bool                                    isEmpty();
        time_t                                  nearEndMs();
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