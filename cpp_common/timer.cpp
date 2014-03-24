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

Timer::Ptr TimerMgr::AddTimer(time_t delayMs, Timer::Callback callback)
{
	auto t = std::make_shared<Timer>(delayMs + static_cast<time_t>(GetTickCount()), callback);
	mTimers.push(t);

	return t;
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