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

    void    foo()
    {
        cout << m_a << endl;
    }

private:
    int m_a;
};

static void foo(void)
{
    cout << "foo" << endl;
}

class Haha
{
public:
    void    test(int a)
    {
        cout << "111" << endl;
    }
};
static void foo1(int& a, const char* hello)
{
    cout << hello << "fuck  " << a << endl;
}

static void foo2(int a, int b)
{}

#include <set>

using namespace Concurrency;
int main()
{
    set<int> fuckset;
    
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    
    auto    mr = make_shared<Rwlist<int>>();
    Rwlist<string>  aaaaa;
    aaaaa.Push("hal");
    int ffff = *fuckset.begin();
    cout << ffff << endl;
    {
        mr->Push(1);
        mr->ForceSyncWrite();
        mr->SyncRead(0);
        while (true)
        {
            auto& i = mr->PopFront();
            if (&i == nullptr)
            {
                break;
            }
            else
            {
                //delete i;
            }
        }
    }
    {
        TimerMgr t;

        std::thread thread(
            [](shared_ptr<Rwlist<int>>  mr){
            DWORD start = GetTickCount();
            int count = 0;
            while (true)
            {
                while (true)
                {
                    auto& i = mr->PopFront();
                    if (&i == nullptr)
                    {
                        mr->SyncRead(0);
                        break;
                    }
                    else
                    {
                       // delete i;
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

        t.AddTimer(1000, [](int a){
            cout << "1000 " << endl;
        }, 1);

        t.AddTimer(2000, []{
            cout << "2000 " << endl;
        });

        Haha* haha = new Haha;
        t.AddTimer(1000, foo1, 1, "hello");
        t.AddTimer(1000, foo);
        t.AddTimer(1000, &Haha::test, haha, 1);

        while (true)
        {
            for (int i = 0; i < 1000; ++i)
            {
                mr->Push(i);
            }
            mr->TrySyncWrite();
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