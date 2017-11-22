#include <brynet/timer/Timer.h>

using namespace std::chrono;
using namespace brynet;

const steady_clock::time_point& Timer::getEndTime() const
{
    return mEndTime;
}

void Timer::cancel()
{
    mActive = false;
}

Timer::Timer(steady_clock::time_point endTime, Callback callback) BRYNET_NOEXCEPT :
    mEndTime(std::move(endTime)), 
    mCallback(std::move(callback)),
    mActive(true)
{
}

void Timer::operator() ()
{
    if (mActive)
    {
        mCallback();
    }
}

void TimerMgr::schedule()
{
    while (!mTimers.empty())
    {
        auto tmp = mTimers.top();
        if (tmp->getEndTime() > steady_clock::now())
        {
            break;
        }

        mTimers.pop();
        tmp->operator() ();
    }
}

bool TimerMgr::isEmpty() const
{
    return mTimers.empty();
}

nanoseconds TimerMgr::nearLeftTime() const
{
    if (mTimers.empty())
    {
        return nanoseconds::zero();
    }

    auto result = mTimers.top()->getEndTime() - steady_clock::now();
    if (result.count() < 0)
    {
        return nanoseconds::zero();
    }

    return result;
}

void TimerMgr::clear()
{
    while (!mTimers.empty())
    {
        mTimers.pop();
    }
}