#define CRTDBG_MAP_ALLOC

#include <windows.h>
#include "../cpp_common/timer.h"
#include "../cpp_common/rwlist.h"
#include <thread>
#include <iostream>
#include <chrono>

using namespace std;

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

using namespace Concurrency;
int main()
{
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	shared_ptr<Rwlist<int>>	mr = make_shared<Rwlist<int>>();
	Rwlist<string>	aaaaa;
    aaaaa.Push("hal");

    {
        std::chrono::microseconds(1);
        std::thread t1([]()
        {
            while (true)
            {
                //cout << "cpp11 thread" << endl;
                std::this_thread::sleep_for(std::chrono::microseconds(0));
            }
        });

        t1.detach();
    }

	{
        mr->Push(1);
        mr->Push(2);
        mr->SyncWrite();
        mr->SyncRead(1);
		while (true)
		{
            int& i = mr->PopFront();
			if (&i == NULL)
			{
				break;
			}
		}
	}
	{
		TimerMgr t;

        std::thread thread(
            [](shared_ptr<Rwlist<int>>	mr){
            DWORD start = GetTickCount();
            int count = 0;
            while (true)
            {
                while (true)
                {
                    int& i = mr->PopFront();
                    if (&i == nullptr)
                    {
                        mr->SyncRead(1);
                        break;
                    }
                    else
                    {
                        count++;
                    }
                }

                DWORD now = GetTickCount();
                if ((now - start) >= 1000)
                {
                    start = now;
                    cout << "count : " << count << endl;
                    count = 0;
                }
            }
        }, mr);

		auto tmp = t.AddTimer(3000, [](){
			cout << "3000 " <<  endl;
		});


		{
			auto tmp = [](int a)
			{
				cout << a << endl;
			};

			tmp(1);

			[](int a)
			{
				cout << a << endl;
			}(1);
		}
		tmp.lock()->Cancel();

		{
			auto a = make_shared<Test>();
			a->inc();
			a->inc();
			a->inc();
			a->inc();

			t.AddTimer(5000, [a] (){
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

		while (true)
		{
			for (int i = 0; i < 1000; ++i)
			{
				mr->Push(1);
			}
			mr->SyncWrite();
			t.Schedule();
            std::this_thread::sleep_for(std::chrono::microseconds(0));
		}

		if (tmp.lock())
		{
			cout << "valid" << endl;
		}
		else
		{
			cout << "is null" << endl;
		}
		cin.get();
	}

	_CrtDumpMemoryLeaks();

	return 0;
}