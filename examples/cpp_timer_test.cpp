#define CRTDBG_MAP_ALLOC

#include <windows.h>
#include "timer.h"
#include <iostream>

using namespace std;

class Test
{
public:
	void    foo(int a, int b)
	{
		cout << "in test::foo" << endl;
		cout << a << "," << b << endl;
	}
};

static void foo(void)
{
	cout << "foo" << endl;
}

static void foo1(int& a, const char* hello)
{
	cout << "in foo1" << endl;
	cout << hello << "fuck  " << a << endl;
}

static void foo2(int a, int b)
{}


int main()
{
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

	TimerMgr t;

	auto tmp = t.AddTimer(3000, [](){
		cout << "in lambda :3000 " << endl;
	});

	tmp.lock()->Cancel();

	t.AddTimer(1000, [](int a){
		cout << "in lambda: 1000, a: " << a << endl;
	}, 1);

	t.AddTimer(2000, []{
		cout << "in lambda: 2000"<< endl;
	});

	Test* test = new Test;
	t.AddTimer(1000, foo1, 1, "hello");
	t.AddTimer(1000, foo);
	auto temp = t.AddTimer(1000, &Test::foo, test, 1, 2);

	while (true)
	{
		t.Schedule();
	}

	_CrtDumpMemoryLeaks();

	return 0;
}