#include "timer.h"

time_t Timer::GetEndMs() const
{
    return mEndMs;
}

void Timer::Cancel()
{
    mActive = false;
}

Timer::Timer(time_t ms, Callback f) : mEndMs(ms), mCallback(f)
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

void TimerMgr::Schedule()
{
    while (!mTimers.empty())
    {
        auto tmp = mTimers.top();

        if (tmp->GetEndMs() < ox_getnowtime())
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

bool TimerMgr::IsEmpty()
{
    return mTimers.empty();
}

time_t TimerMgr::NearEndMs()
{
    if (mTimers.empty())
    {
        return 0;
    }
    else
    {
        time_t tmp = mTimers.top()->GetEndMs() - ox_getnowtime();
        return (tmp < 0 ? 0 : tmp);
    }
}