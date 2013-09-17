#include <iostream>
using namespace std;

#include "thread.h"
#include "wrap_server.h"

class MyServer : public WrapServer
{
	void	onConnected(int)
	{
		cout << "enter" << endl;
	}

	void	onDisconnected(int)
	{
		cout << "onDisconnected" << endl;
	}

	void	onRecvdata(int index, const char* data, int len)
	{
		sendTo(index, data, len);
	}

	int	check(const char* buffer, int len)
	{
		return len;
	}
};

int main()
{
	MyServer m;
	m.create("127.0.0.1", 4002, 1, 1024);
	while(true)
	{
		m.poll(1);
	}
	return 0;
}
