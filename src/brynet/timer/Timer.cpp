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

std::chrono::nanoseconds Timer::getLeftTime() const
{
    return getLastTime() - (steady_clock::now() - getStartTime());
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
        if (tmp->getLeftTime() > nanoseconds::zero())
        {
            break;
        }

        mTimers.pop();
        (*tmp)();
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

    auto result = mTimers.top()->getLeftTime();
    if (result < nanoseconds::zero())
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