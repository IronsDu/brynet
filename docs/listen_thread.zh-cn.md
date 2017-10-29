# 概述
`ListenThread `是一个用于接受外部连接的类.源代码见:[ListenThread.h](https://github.com/IronsDu/brynet/blob/master/src/brynet/net/ListenThread.h).

# 接口

- `ListenThread::Create(void)`
	
	创建`ListenThread::PTR`智能指针对象，此对象用于后续工作.

- `ListenThread::startThread(bool isIPV6, const std::string& ip, int port, CB cb)`
	
	(线程安全)此成员函数用于开启工作/监听线程，`isIPV6`表示是否使用ipv6。
	当收到外部链接时会回调cb。CB的类型是`std::function<void(sock fd)>`。

- `ListenThread::closeListenThread(void)`
	
	(线程安全)此成员函数用于停止工作线程,这个函数需要等待网络线程完全结束才会返回.

## 示例
```C++
auto listenThread = ListenThread::Create();
listenThread->startThread(false,
	"0.0.0.0",
	9999,
	[](sock fd) {
		std::cout << "accepted connection" << std::endl;
		// 在此我们就可以将 fd 用于网络库的其他部分,比如用于`TCPService`
	});

// wait 2s
std::this_thread::sleep_for(2s);

listenThread->closeListenThread();
```
