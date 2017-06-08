#ifndef BRYNET_MSGQUEUE_H_
#define BRYNET_MSGQUEUE_H_

#include <mutex>
#include <condition_variable>
#include <deque>

#include "NonCopyable.h"

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
            mMutex.lock();
            mReadList.clear();
            mWriteList.clear();
            mSharedList.clear();
            mMutex.unlock();
        }

        void    push(const T& t)
        {
            mWriteList.push_back(t);
        }

        void    push(T&& t)
        {
            mWriteList.push_back(std::move(t));
        }

        /*  同步写缓冲到共享队列(共享队列必须为空)    */
        void    trySyncWrite()
        {
            if (!mWriteList.empty() && mSharedList.empty())
            {
                mMutex.lock();

                if (!mWriteList.empty() && mSharedList.empty())
                {
                    mSharedList.swap(mWriteList);
                    mCond.notify_one();
                }

                mMutex.unlock();
            }
        }

        /*  强制同步    */
        void    forceSyncWrite()
        {
            if (!mWriteList.empty())
            {
                if (mSharedList.empty())
                {
                    /*  如果共享队列为空，则进行交换  */
                    trySyncWrite();
                }
                else
                {
                    mMutex.lock();

                    if (!mWriteList.empty())
                    {
                        /*  强制写入    */
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

                    mMutex.unlock();
                }
            }
        }

        bool      popFront(T& data)
        {
            bool ret = false;

            if (!mReadList.empty())
            {
                T& tmp = mReadList.front();
                data = std::move(tmp);
                mReadList.pop_front();
                ret = true;
            }

            return ret;
        }

        bool      popBack(T& data)
        {
            bool ret = false;

            if (!mReadList.empty())
            {
                T& tmp = mReadList.back();
                data = std::move(tmp);
                mReadList.pop_back();
                ret = true;
            }

            return ret;
        }

        /*  从共享队列同步到读缓冲区(必须读缓冲区为空时) */
        void    syncRead(size_t waitMicroSecond)
        {
            if (mReadList.empty())
            {
                if (waitMicroSecond > 0)
                {
                    std::unique_lock<std::mutex>    tmp(mMutex);
                    mCond.wait_for(tmp, std::chrono::microseconds(waitMicroSecond));
                }

                mMutex.lock();

                if (mReadList.empty() && !mSharedList.empty())
                {
                    mSharedList.swap(mReadList);
                }

                mMutex.unlock();
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

        /*  写缓冲 */
        Container                       mWriteList;
        /*  共享队列    */
        Container                       mSharedList;
        /*  读缓冲区    */
        Container                       mReadList;
    };
}

#endif