#ifndef BRYNET_MSGQUEUE_H_
#define BRYNET_MSGQUEUE_H_

#include <mutex>
#include <condition_variable>
#include <deque>

#include <brynet/utils/NonCopyable.h>

/* 1:1 rw msg queue */
namespace brynet
{
    template<typename T>
    class MsgQueue : public NonCopyable
    {
    public:
        typedef std::deque<T>   Container;

        MsgQueue()
        {
        }

        virtual ~MsgQueue()
        {
            clear();
        }

        void    clear()
        {
            std::lock_guard<std::mutex> lck(mMutex);
            mReadList.clear();
            mWriteList.clear();
            mSharedList.clear();
        }

        void    push(const T& t)
        {
            mWriteList.push_back(t);
        }

        void    push(T&& t)
        {
            mWriteList.push_back(std::move(t));
        }

        void    trySyncWrite()
        {
            if (mWriteList.empty() || !mSharedList.empty())
            {
                return;
            }

            std::lock_guard<std::mutex> lck(mMutex);
            if (!mWriteList.empty() && mSharedList.empty())
            {
                mSharedList.swap(mWriteList);
                mCond.notify_one();
            }
        }

        void    forceSyncWrite()
        {
            if (mWriteList.empty())
            {
                return;
            }
            
            if (mSharedList.empty())
            {
                trySyncWrite();
                return;
            }
            
            std::lock_guard<std::mutex> lck(mMutex);
            if (mWriteList.empty())
            {
                return;
            }
            
            if (mWriteList.size() > mSharedList.size())
            {
                for (auto it = mSharedList.rbegin(); it != mSharedList.rend(); ++it)
                {
                    mWriteList.push_front(std::move(*it));
                }
                mSharedList.clear();
                mSharedList.swap(mWriteList);
            }
            else
            {
                for (auto& x : mWriteList)
                {
                    mSharedList.push_back(std::move(x));
                }
                mWriteList.clear();
            }

            mCond.notify_one();
        }

        bool      popFront(T& data)
        {
            if (mReadList.empty())
            {
                return false;
            }
            
            T& tmp = mReadList.front();
            data = std::move(tmp);
            mReadList.pop_front();

            return true;
        }

        bool      popBack(T& data)
        {
            if (mReadList.empty())
            {
                return false;
            }

            T& tmp = mReadList.back();
            data = std::move(tmp);
            mReadList.pop_back();

            return true;
        }

        void    syncRead(std::chrono::microseconds waitMicroSecond)
        {
            if (!mReadList.empty())
            {
                return;
            }

            if (waitMicroSecond != std::chrono::microseconds::zero())
            {
                std::unique_lock<std::mutex>    tmp(mMutex);
                mCond.wait_until(tmp, std::chrono::steady_clock::now() + waitMicroSecond);
            }

            std::lock_guard<std::mutex> lck(mMutex);
            if (mReadList.empty() && !mSharedList.empty())
            {
                mSharedList.swap(mReadList);
            }
        }

        size_t  sharedListSize() const
        {
            return mSharedList.size();
        }

        size_t  readListSize() const
        {
            return mReadList.size();
        }

        size_t  writeListSize() const
        {
            return mWriteList.size();
        }

    private:
        std::mutex                      mMutex;
        std::condition_variable         mCond;

        Container                       mWriteList;
        Container                       mSharedList;
        Container                       mReadList;
    };
}

#endif