#ifndef _TIMER_H
#define _TIMER_H

#include <functional>
#include <queue>
#include <memory>
#include <vector>

class TimerMgr;

class Timer
{
public:
	typedef std::shared_ptr<Timer>			Ptr;
	typedef std::weak_ptr<Timer>			WeakPtr;
	typedef std::function<void(void)>		Callback;

	time_t									GetEndMs() const;
	void									Cancel();

private:
	Timer(time_t ms, Callback f);

	void operator()							();

private:
	bool									mActive;
	Callback								mCallback;
	time_t									mEndMs;


private:
	friend class TimerMgr;

	template<typename _T, class... _Types>
	friend std::shared_ptr<_T>				std::make_shared(_Types&&...);

	friend class std::_Ref_count_obj<Timer>;

	friend class std::shared_ptr<Timer>;
};

class TimerMgr
{
public:
	Timer::WeakPtr							AddTimer(time_t delayMs, Timer::Callback callback);

	void									Schedule();

	bool									IsEmpty();

private:
	class CompareTimer
	{
	public:
		bool operator() (const Timer::Ptr& left, const Timer::Ptr& right) const
		{
			return left->GetEndMs() > right->GetEndMs();
		}
	};

	std::priority_queue<Timer::Ptr, std::vector<Timer::Ptr>, CompareTimer>	mTimers;
};

#endif