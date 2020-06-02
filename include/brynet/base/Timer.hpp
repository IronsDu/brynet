#pragma once

#include <functional>
#include <queue>
#include <memory>
#include <vector>
#include <chrono>
#include <mutex>

#include <brynet/base/Noexcept.hpp>

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
            :
            mCallback(std::move(callback)),
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

        std::chrono::nanoseconds    getLeftTime() const
        {
            const auto now = std::chrono::steady_clock::now();
            return getLastTime() - (now - getStartTime());
        }

        void    cancel()
        {
            std::call_once(mExecuteOnceFlag, [this]() {
                    mCallback = nullptr;
                });
        }

    private:
        void            operator() ()
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
        std::once_flag                                  mExecuteOnceFlag;
        Callback                                        mCallback;
        const std::chrono::steady_clock::time_point     mStartTime;
        const std::chrono::nanoseconds                  mLastTime;

        friend class TimerMgr;
    };

    class TimerMgr final
    {
    public:
        using Ptr = std::shared_ptr<TimerMgr>;

        template<typename F, typename ...TArgs>
        Timer::WeakPtr  addTimer(
            std::chrono::nanoseconds timeout,
            F&& callback, 
            TArgs&& ...args)
        {
            auto timer = std::make_shared<Timer>(
                std::chrono::steady_clock::now(),
                std::chrono::nanoseconds(timeout),
                std::bind(std::forward<F>(callback), std::forward<TArgs>(args)...));
            mTimers.push(timer);

            return timer;
        }

        void        addTimer(const Timer::Ptr& timer)
        {
            mTimers.push(timer);
        }

        void        schedule()
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

        bool        isEmpty() const
        {
            return mTimers.empty();
        }

        // if timer empty, return zero
        std::chrono::nanoseconds    nearLeftTime() const
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

        void    clear()
        {
            while (!mTimers.empty())
            {
                mTimers.pop();
            }
        }

    private:
        class CompareTimer
        {
        public:
            bool operator() (const Timer::Ptr& left, 
                const Timer::Ptr& right) const
            {
                const auto startDiff = left->getStartTime() - right->getStartTime();
                const auto lastDiff = left->getLastTime() - right->getLastTime();
                const auto diff = startDiff.count() + lastDiff.count();
                return diff > 0;
            }
        };

        std::priority_queue<Timer::Ptr, std::vector<Timer::Ptr>, CompareTimer>  mTimers;
    };

} }