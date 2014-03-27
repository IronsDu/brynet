#include <Windows.h>

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

        if (tmp->GetEndMs() < GetTickCount())
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