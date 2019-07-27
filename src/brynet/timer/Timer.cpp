#include <brynet/timer/Timer.h>

namespace brynet { namespace timer {

    const std::chrono::steady_clock::time_point& Timer::getStartTime() const
    {
        return mStartTime;
    }

    const std::chrono::nanoseconds& Timer::getLastTime() const
    {
        return mLastTime;
    }

    std::chrono::nanoseconds Timer::getLeftTime() const
    {
        return getLastTime() - (std::chrono::steady_clock::now() - getStartTime());
    }

    void Timer::cancel()
    {
        std::call_once(mExecuteOnceFlag, [this]() {
                mCallback = nullptr;
            });
    }

    Timer::Timer(std::chrono::steady_clock::time_point startTime,
        std::chrono::nanoseconds lastTime,
        Callback&& callback) BRYNET_NOEXCEPT
        :
        mCallback(std::forward<Callback>(callback)),
        mStartTime(startTime),
        mLastTime(lastTime)
    {
    }

    void Timer::operator() ()
    {
        std::call_once(mExecuteOnceFlag, [this]() {
                mCallback();
                mCallback = nullptr;
            });
    }

    void TimerMgr::schedule()
    {
        while (!mTimers.empty())
        {
            auto tmp = mTimers.top();
            if (tmp->getLeftTime() > std::chrono::nanoseconds::zero())
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

    std::chrono::nanoseconds TimerMgr::nearLeftTime() const
    {
        if (mTimers.empty())
        {
            return std::chrono::nanoseconds::zero();
        }

        auto result = mTimers.top()->getLeftTime();
        if (result < std::chrono::nanoseconds::zero())
        {
            return std::chrono::nanoseconds::zero();
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

} }
