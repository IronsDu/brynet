#ifndef _RWLIST_H
#define _RWLIST_H

#include <mutex>
#include <condition_variable>

template<typename T>
class Rwlist
{
public:
	typedef std::deque<T>	Container;

    Rwlist() : mLock(mMutex, std::defer_lock)
    {}

	void	Push(T& t)
	{
		mWriteList.push_back(t);
	}

	void	Push(T&& t)
	{
		mWriteList.push_back(t);
	}

    T&	PopFront()
	{
        if (!mReadList.empty())
        {
            T& ret = *(mReadList.begin());
            mReadList.pop_front();
            return ret;
        }
        else
        {
            return *(T*)nullptr;
        }
	}

	/*	同步写缓冲到共享队列(共享队列必须为空)	*/
	void	SyncWrite()
	{
		if (mSharedList.empty())
		{
            mLock.lock();

			mSharedList.swap(mWriteList);
            mCond.notify_one();

            mLock.unlock();
		}
	}

	/*	从共享队列同步到读缓冲区(必须读缓冲区为空时)	*/
    void	SyncRead(int waitMicroSecond)
	{
		if (mReadList.empty())
		{
            mLock.lock();

            if (mSharedList.empty() && waitMicroSecond > 0)
			{
				/*  如果共享队列没有数据且timeout大于0则需要等待通知,否则直接进行同步    */
                mCond.wait_for(mLock, std::chrono::microseconds(waitMicroSecond), [](){return false; });
			}

			if (!mSharedList.empty())
			{
				mSharedList.swap(mReadList);
			}

            mLock.unlock();
		}
	}

private:
    std::mutex                      mMutex;
    std::unique_lock<std::mutex>    mLock;
    std::condition_variable         mCond;

	/*	写缓冲	*/
	Container	                    mWriteList;
	/*	共享队列	*/
	Container	                    mSharedList;
	/*	读缓冲区	*/
	Container	                    mReadList;
};

#endif