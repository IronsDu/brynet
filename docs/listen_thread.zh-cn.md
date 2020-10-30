# 概述
`ListenThread `是一个用于接受外部连接的类.源代码见:[ListenThread.hpp](https://github.com/IronsDu/brynet/blob/master/include/brynet/net/ListenThread.hpp).

# 接口

- `ListenThread::Create(bool isIPV6, const std::string& ip, int port, const AccepCallback& callback, const std::vector<TcpSocketProcessCallback>& process = {})`
	
	创建`ListenThread::Ptr`智能指针对象，此对象用于后续工作. `callback`是连接进入的回调，在其中使用传进的`TcpSocket::Ptr`，当`callback`为nullptr时会产生异常。</br>
	`process `也是连接进入时的回调，但它的参数类型是`TcpSocket&`，主要用于一些socket选项设置。

- `ListenThread::startThread()`
	
	(线程安全)此成员函数用于开启工作/监听线程，如果`startThread`失败（比如监听失败）会产生异常。

- `ListenThread::stopListen(void)`
	
	(线程安全)此成员函数用于停止工作线程,这个函数需要等待监听线程完全结束才会返回.

## 示例
```C++
auto listenThread = ListenThread::Create(false,
	"0.0.0.0",
	9999,
	[](TcpSocket::Ptr socket) {
		std::cout << "accepted connection" << std::endl;
		// 在此我们就可以将 socket 用于网络库的其他部分,比如用于`TCPService`
	},
	{
		[](TcpSocket& socket) {
			socket.setNodelay(); // and do some setting.
		}
	});
listenThread->startThread();

// wait 2s
std::this_thread::sleep_for(2s);

listenThread->stopListen();
```

# 注意事项
- 请小心`ListenThread::startThread`产生异常
