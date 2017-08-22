#include <brynet/timer/Timer.h>

using namespace brynet;

const std::chrono::steady_clock::time_point& Timer::getEndMs() const
{
    return mEndTime;
}

void Timer::cancel()
{
    mActive = false;
}

Timer::Timer(std::chrono::steady_clock::time_point endTime, Callback callback) noexcept : mEndTime(std::move(endTime)), mCallback(std::move(callback))
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

        if (tmp->getEndMs() > std::chrono::steady_clock::now())
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

/* 返回定时器管理器中最近的一个定时器还需多久到期(如果定时器为空或已经到期则返回0) */
std::chrono::milliseconds TimerMgr::nearEndMs() const
{
    if (mTimers.empty())
    {
        return std::chrono::milliseconds();
    }

    auto result = std::chrono::duration_cast<std::chrono::milliseconds>(mTimers.top()->getEndMs() - std::chrono::steady_clock::now());
    if (result.count() < 0)
    {
        return std::chrono::milliseconds();
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