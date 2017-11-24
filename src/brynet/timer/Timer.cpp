#include <brynet/timer/Timer.h>

using namespace std::chrono;
using namespace brynet;

const steady_clock::time_point& Timer::getStartTime() const
{
    return mStartTime;
}

const std::chrono::nanoseconds& Timer::getLastTime() const
{
    return mLastTime;
}

void Timer::cancel()
{
    mActive = false;
}

Timer::Timer(std::chrono::steady_clock::time_point startTime,
    std::chrono::nanoseconds lastTime,
    Callback callback) BRYNET_NOEXCEPT :
    mStartTime(std::move(startTime)),
    mLastTime(std::move(lastTime)),
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
        if ((steady_clock::now() - tmp->getStartTime()) < tmp->getLastTime())
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

    auto result = steady_clock::now() - 
        mTimers.top()->getStartTime() +
        mTimers.top()->getLastTime();
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