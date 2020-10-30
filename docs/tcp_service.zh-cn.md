# 概述
`TCPService`是`brynet`中对`TcpConnection`和`EventLoop`的封装组合。
源代码见:[TCPService.h](https://github.com/IronsDu/brynet/blob/master/include/brynet/net/TCPService.hpp).</br>

# 接口

- `TcpService::Create(void)`
  

    此静态函数用于创建网络服务对象，且只能使用此接口创建`TcpService`对象。</br>
    用户通过服务对象操作网络会话。

- `TcpService::startWorkerThread(size_t threadNum, FRAME_CALLBACK callback = nullptr)`

    (线程安全)开启工作线程，每一个工作线程有一个`EventLoop`负责事件检测。


- `TcpService::addTcpConnection(TcpSocket::Ptr socket, Options...)`

    (线程安全),将一个TcpConnection交给TcpService管理,其中Options请查阅`AddSocketOption的WithXXX系列函数`。


## 示例
```C++
auto service = TcpService::Create();
// use blocking connect for test
auto fd = ox_socket_connect(false, ip, port);
auto socket = TcpSocket::Create(fd, false);

service->addTcpConnection(socket, AddSocketOption::WithMaxRecvBufferSize(1024*1024));

std::this_thread::sleep_for(2s);
```
