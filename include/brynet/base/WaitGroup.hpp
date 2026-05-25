#pragma once

#include <atomic>
#include <brynet/base/NonCopyable.hpp>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>

namespace brynet { namespace base {

class WaitGroup : public NonCopyable
{
public:
    typedef std::shared_ptr<WaitGroup> Ptr;

    static Ptr Create()
    {
        struct make_shared_enabler : public WaitGroup
        {
        };
        return std::make_shared<make_shared_enabler>();
    }

public:
    void add(int i = 1)
    {
        std::unique_lock<std::mutex> l(mMutex);
        mCounter += i;
    }

    void done()
    {
        std::unique_lock<std::mutex> l(mMutex);
        --mCounter;
        if (mCounter <= 0)
        {
            mCond.notify_all();
        }
    }

    void wait()
    {
        std::unique_lock<std::mutex> l(mMutex);
        mCond.wait(l, [&] {
            return mCounter <= 0;
        });
    }

    template<class Rep, class Period>
    bool wait(const std::chrono::duration<Rep, Period>& timeout)
    {
        std::unique_lock<std::mutex> l(mMutex);
        return mCond.wait_for(l, timeout, [&] {
            return mCounter <= 0;
        });
    }

private:
    WaitGroup()
        : mCounter(0)
    {
    }

    virtual ~WaitGroup() = default;

private:
    std::mutex mMutex;
    int mCounter;
    std::condition_variable mCond;
};
}}// namespace brynet::base
