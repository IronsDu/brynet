#ifndef _TIMER_H
#define _TIMER_H

#include <functional>
#include <queue>
#include <memory>
#include <vector>

#include "systemlib.h"

class TimerMgr;

class Timer
{
public:
    typedef std::shared_ptr<Timer>          Ptr;
    typedef std::weak_ptr<Timer>            WeakPtr;
    typedef std::function<void(void)>       Callback;

    time_t                                  GetEndMs() const;
    void                                    Cancel();

    Timer(time_t ms, Callback f);

    void operator()                         ();

private:
    bool                                    mActive;
    Callback                                mCallback;
    time_t                                  mEndMs;
};

class TimerMgr
{
public:
    typedef std::shared_ptr<TimerMgr>   PTR;

    template<typename F, typename ...TArgs>
    Timer::WeakPtr                          AddTimer(time_t delayMs, F callback, TArgs&& ...args)
    {
        auto t = std::make_shared<Timer>(delayMs + static_cast<time_t>(ox_getnowtime()),
                                            std::bind(callback, std::forward<TArgs>(args)...));
        mTimers.push(t);

        return t;
    }

    void                                    Schedule();

    bool                                    IsEmpty();

    time_t                                  NearEndMs();

private:
    class CompareTimer
    {
    public:
        bool operator() (const Timer::Ptr& left, const Timer::Ptr& right) const
        {
            return left->GetEndMs() > right->GetEndMs();
        }
    };

    std::priority_queue<Timer::Ptr, std::vector<Timer::Ptr>, CompareTimer>  mTimers;
};

#endif