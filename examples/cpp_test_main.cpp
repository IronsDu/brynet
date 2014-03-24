#define CRTDBG_MAP_ALLOC

#include "../cpp_common/timer.h"
#include <functional>
#include <queue>
#include <vector>
#include <iostream>
#include <memory>
#include <time.h>
#include <deque>

#include <Windows.h>

using namespace std;


class Mutex
{
public:
	Mutex()
	{
		InitializeCriticalSection(&mMutex);
	}

	~Mutex()
	{
		DeleteCriticalSection(&mMutex);
	}

	void	Lock()
	{
		EnterCriticalSection(&mMutex);
	}

	void	UnLock()
	{
		LeaveCriticalSection(&mMutex);
	}

private:
	CRITICAL_SECTION    mMutex;
};

class CondMutex
{
public:
	CondMutex()
	{
		mCond = CreateEvent(NULL, FALSE, FALSE, NULL);
	}

	~CondMutex()
	{
		CloseHandle(mCond);
	}

	void	Wait(Mutex& mutex)
	{
		mutex.UnLock();
		WaitForSingleObject(mCond, INFINITE);
		mutex.Lock();
	}

	void	timeWait(Mutex& mutex, int timeover)
	{
		mutex.UnLock();
		WaitForSingleObject(mCond, timeover);
		mutex.Lock();
	}
	
	void	Singal()
	{
		SetEvent(mCond);
	}
private:
	HANDLE  mCond;
};

template<typename T>
class Rwlist
{
public:
	typedef std::deque<T>	Container;

	Rwlist()
	{	
		mWriteList = new Container();
		mSharedList = new Container();
		mReadList = new Container();
	}

	~Rwlist()
	{
		delete mWriteList;
		delete mSharedList;
		delete mReadList;

		mWriteList = nullptr;
		mSharedList = nullptr;
		mReadList = nullptr;
	}

	void	push(T t)
	{
		mWriteList->push_back(t);
	}

	T&	front()
	{
		if (!mReadList->empty())
		{
			return *(mReadList->begin());
		}
		else
		{
			return *(T*)nullptr;
		}
	}

	void	pop_front()
	{
		mReadList->pop_front();
	}

	/*	同步写缓冲到共享队列(共享队列必须为空)	*/
	void	syncWrite()
	{
		if (mSharedList->empty())
		{
			mMutex.Lock();

			auto* temp = mSharedList;
			mSharedList = mWriteList;
			mWriteList = temp;

			mCond.Singal();

			mMutex.UnLock();
		}
	}

	/*	从共享队列同步到读缓冲区(必须读缓冲区为空时)	*/
	void	syncRead(int timeout)
	{
		if (mReadList->empty())
		{
			mMutex.Lock();

			if (mSharedList->empty() && timeout > 0)
			{
				/*  如果共享队列没有数据且timeout大于0则需要等待通知,否则直接进行同步    */
				mCond.timeWait(mMutex, timeout);
			}

			if (!mSharedList->empty())
			{
				auto* temp = mSharedList;
				mSharedList = mReadList;
				mReadList = temp;
			}

			mMutex.UnLock();
		}
	}

private:
	Mutex		mMutex;
	CondMutex	mCond;

	/*	写缓冲	*/
	Container*	mWriteList;
	/*	共享队列	*/
	Container*	mSharedList;
	/*	读缓冲区	*/
	Container*	mReadList;
};

class Test
{
public:
	Test()
	{
		m_a = 1;
	}

	void inc()
	{
		m_a++;
	}

	~Test()
	{
		m_a = 2;
	}

	void	foo()
	{
		cout << m_a << endl;
	}

private:
	int	m_a;
};

static void foo(void)
{
	cout << "foo" << endl;
}

int main()
{
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

	{
		Rwlist<int>	mr;
		mr.push(1);
		mr.push(2);
		mr.syncWrite();
		mr.syncRead(1);
		while (true)
		{
			int& i = mr.front();
			if (&i == NULL)
			{
				break;
			}
			mr.pop_front();
		}
	}
	{
		TimerMgr t;
		

		Timer::Ptr tmp = t.AddTimer(3000, []{
			cout << "3000 " <<  endl;
		});

		tmp->Cancel();

		{
			auto a = make_shared<Test>();
			a->inc();
			a->inc();
			a->inc();
			a->inc();

			t.AddTimer(5000, [a]{
				cout << "5000 " <<  endl;
				a->foo();
			});
		}

		t.AddTimer(1000, []{
			cout << "1000 " << endl;
		});

		t.AddTimer(2000, []{
			cout << "2000 " << endl;
		});

		t.AddTimer(0, foo);
		t.AddTimer(0, foo);
		t.AddTimer(0, foo);

		while (!t.IsEmpty())
		{
			t.Schedule();
			Sleep(0);
		}
		cin.get();
	}

	_CrtDumpMemoryLeaks();

	return 0;
}