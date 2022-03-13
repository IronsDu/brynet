# 概述
`ITCPService`是`brynet`中对`TcpConnection`和`EventLoop`的封装抽象类型。它的派生类分为两个：`IOThreadTcpService`和`EventLoopTcpService`。


`IOThreadTcpService`开启额外的IO工作线程，并使用其独立的EventLoop处理网络。

`EventLoopTcpService`则不会开启额外的线程，而是直接使用用户设定的EventLoop来处理网络，其线程就是调用`loop`函数来调度此EventLoop的线程，
它通常可以用于不需要极致性能的场景，比如直接在主线程处理IO。

源代码见:[TCPService.h](https://github.com/IronsDu/brynet/blob/master/include/brynet/net/TCPService.hpp).</br>

# 接口

- `ITcpService::addTcpConnection(TcpSocket::Ptr socket, Options...)`

    这是ITcpService抽象类的接口，其用途是将一个TcpConnection交给具体的ITcpService管理,其中Options请查阅`AddSocketOption的WithXXX系列函数`。

- `IOThreadTcpService::Create(void)`
  

    此静态函数用于创建网络服务对象，且只能使用此接口创建`ITcpService`对象。</br>
    用户通过服务对象操作网络会话。

- `IOThreadTcpService::startWorkerThread(size_t threadNum, FRAME_CALLBACK callback = nullptr)`

    (线程安全)开启IO工作线程，每一个工作线程有一个`EventLoop`负责事件检测。


- `IOThreadTcpService::stopWorkerThread()`

    (线程安全)关闭IO工作线程。

    
- `EventLoopTcpService::Create(EventLoop::Ptr eventLoop)`

    根据用户指定的EventLoop构造一个单线程TcpService管理器。



## 示例
```C++
auto service = IOThreadTcpService::Create();
// use blocking connect for test
auto fd = ox_socket_connect(false, ip, port);
auto socket = TcpSocket::Create(fd, false);

service->addTcpConnection(socket, AddSocketOption::WithMaxRecvBufferSize(1024*1024));

std::this_thread::sleep_for(2s);
```
