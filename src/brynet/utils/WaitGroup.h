#ifndef _WAIT_GROUP_H
#define _WAIT_GROUP_H

#include <mutex>
#include <atomic>
#include <memory>
#include <condition_variable>
#include <chrono>

#include <brynet/utils/NonCopyable.h>

namespace brynet
{
    class WaitGroup : public brynet::NonCopyable
    {
    public:
        typedef std::shared_ptr<WaitGroup> PTR;

        static PTR Create()
        {
            struct make_shared_enabler : public WaitGroup {};
            return std::make_shared<make_shared_enabler>();
        }

    public:
        void    add(int i = 1)
        {
            mCounter += i;
        }

        void    done()
        {
            mCounter--;
            mCond.notify_all();
        }

        void    wait()
        {
            std::unique_lock<std::mutex> l(mMutex);
            mCond.wait(l, [&] { return mCounter <= 0; });
        }

        template<class Rep, class Period>
        void    wait(const std::chrono::duration<Rep, Period>& timeout)
        {
            std::unique_lock<std::mutex> l(mMutex);
            mCond.wait_for(l, timeout, [&] {
                return mCounter <= 0;
            });
        }

    private:
        WaitGroup()
            :
            mCounter(0)
        {
        }

        virtual ~WaitGroup()
        {}

    private:
        std::mutex              mMutex;
        std::atomic<int>        mCounter;
        std::condition_variable mCond;
    };
}

#endif
