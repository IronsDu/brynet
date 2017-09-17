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

Timer::Timer(steady_clock::time_point endTime, Callback callback) noexcept : 
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

/* 返回定时器管理器中最近的一个定时器还需多久到期(如果定时器为空或已经到期则返回zero) */
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