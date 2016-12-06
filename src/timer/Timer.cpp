#include "Timer.h"

using namespace dodo;

time_t Timer::getEndMs() const
{
    return mEndMs;
}

void Timer::cancel()
{
    mActive = false;
}

Timer::Timer(time_t ms, Callback callback) : mEndMs(ms), mCallback(callback)
{
    mActive = true;
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

        if (tmp->getEndMs() < ox_getnowtime())
        {
            mTimers.pop();
            tmp->operator() ();
        }
        else
        {
            break;
        }
    }
}

bool TimerMgr::isEmpty()
{
    return mTimers.empty();
}

time_t TimerMgr::nearEndMs()
{
    if (mTimers.empty())
    {
        return 0;
    }
    else
    {
        time_t tmp = mTimers.top()->getEndMs() - ox_getnowtime();
        return (tmp < 0 ? 0 : tmp);
    }
}

void TimerMgr::clear()
{
    while (!mTimers.empty())
    {
        mTimers.pop();
    }
}