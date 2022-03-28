#pragma once

#include <atomic>
#include <brynet/base/Noexcept.hpp>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <vector>

namespace brynet { namespace base {

class TimerMgr;

class Timer final
{
public:
    using Ptr = std::shared_ptr<Timer>;
    using WeakPtr = std::weak_ptr<Timer>;
    using Callback = std::function<void(void)>;

    Timer(std::chrono::steady_clock::time_point startTime,
          std::chrono::nanoseconds lastTime,
          Callback&& callback) BRYNET_NOEXCEPT
        : mCallback(std::move(callback)),
          mStartTime(startTime),
          mLastTime(lastTime)
    {
    }

    const std::chrono::steady_clock::time_point& getStartTime() const
    {
        return mStartTime;
    }

    const std::chrono::nanoseconds& getLastTime() const
    {
        return mLastTime;
    }

    std::chrono::nanoseconds getLeftTime() const
    {
        const auto now = std::chrono::steady_clock::now();
        return getLastTime() - (now - getStartTime());
    }

    void cancel()
    {
        std::call_once(mExecuteOnceFlag, [this]() {
            mCallback = nullptr;
        });
    }

private:
    void operator()()
    {
        Callback callback;
        std::call_once(mExecuteOnceFlag, [&callback, this]() {
            callback = std::move(mCallback);
            mCallback = nullptr;
        });

        if (callback != nullptr)
        {
            callback();
        }
    }

private:
    std::once_flag mExecuteOnceFlag;
    Callback mCallback;
    const std::chrono::steady_clock::time_point mStartTime;
    const std::chrono::nanoseconds mLastTime;

    friend class TimerMgr;
};

class RepeatTimer final
{
public:
    using Ptr = std::shared_ptr<RepeatTimer>;

    RepeatTimer()
    {
        mCancel.store(false);
    }

    void cancel()
    {
        mCancel.store(true);
    }

    bool isCancel() const
    {
        return mCancel.load();
    }

private:
    std::atomic_bool mCancel;
};

class TimerMgr final : public std::enable_shared_from_this<TimerMgr>
{
public:
    using Ptr = std::shared_ptr<TimerMgr>;

    template<typename F, typename... TArgs>
    Timer::WeakPtr addTimer(
            std::chrono::nanoseconds timeout,
            F&& callback,
            TArgs&&... args)
    {
        auto timer = std::make_shared<Timer>(
                std::chrono::steady_clock::now(),
                std::chrono::nanoseconds(timeout),
                std::bind(std::forward<F>(callback), std::forward<TArgs>(args)...));
        mTimers.push(timer);

        return timer;
    }

    template<typename F, typename... TArgs>
    RepeatTimer::Ptr addIntervalTimer(
            std::chrono::nanoseconds interval,
            F&& callback,
            TArgs&&... args)
    {
        auto sharedThis = shared_from_this();
        auto repeatTimer = std::make_shared<RepeatTimer>();
        auto wrapperCallback = std::bind(std::forward<F>(callback), std::forward<TArgs>(args)...);
        addTimer(interval, [sharedThis, interval, wrapperCallback, repeatTimer]() {
            stubRepeatTimerCallback(sharedThis, interval, wrapperCallback, repeatTimer);
        });

        return repeatTimer;
    }

    void addTimer(const Timer::Ptr& timer)
    {
        mTimers.push(timer);
    }

    void schedule()
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

    bool isEmpty() const
    {
        return mTimers.empty();
    }

    // if timer empty, return zero
    std::chrono::nanoseconds nearLeftTime() const
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

    void clear()
    {
        while (!mTimers.empty())
        {
            mTimers.pop();
        }
    }

    template<typename F, typename... TArgs>
    void helperAddIntervalTimer(
            RepeatTimer::Ptr repeatTimer,
            std::chrono::nanoseconds interval,
            F&& callback,
            TArgs&&... args)
    {
        auto sharedThis = shared_from_this();
        auto wrapperCallback = std::bind(std::forward<F>(callback), std::forward<TArgs>(args)...);
        addTimer(interval, [sharedThis, interval, wrapperCallback, repeatTimer]() {
            stubRepeatTimerCallback(sharedThis, interval, wrapperCallback, repeatTimer);
        });
    }

private:
    static void stubRepeatTimerCallback(TimerMgr::Ptr timerMgr,
                                        std::chrono::nanoseconds interval,
                                        std::function<void()> callback,
                                        RepeatTimer::Ptr repeatTimer)
    {
        if (repeatTimer->isCancel())
        {
            return;
        }
        callback();
        timerMgr->addTimer(interval, [timerMgr, interval, callback, repeatTimer]() {
            stubRepeatTimerCallback(timerMgr, interval, callback, repeatTimer);
        });
    }

private:
    class CompareTimer
    {
    public:
        bool operator()(const Timer::Ptr& left,
                        const Timer::Ptr& right) const
        {
            const auto startDiff = left->getStartTime() - right->getStartTime();
            const auto lastDiff = left->getLastTime() - right->getLastTime();
            const auto diff = startDiff.count() + lastDiff.count();
            return diff > 0;
        }
    };

    std::priority_queue<Timer::Ptr, std::vector<Timer::Ptr>, CompareTimer> mTimers;
};

}}// namespace brynet::base
