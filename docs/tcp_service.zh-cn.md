# 概述
`TCPService`是`brynet`中对`DataSocket`和`EventLoop`的封装组合。
源代码见:[TCPService.h](https://github.com/IronsDu/brynet/blob/master/src/brynet/net/TCPService.h).</br>
其主要意图是以`int64_t`标识网络会话对象，避免用户直接接触`DataSocket`裸指针和`EventLoop`。</br>
使用更安全。

# 接口

- `TcpService::Create(void)`
    

    此静态函数用于创建网络服务对象，且只能使用此接口创建`TcpService`对象。</br>
    用户通过服务对象操作网络会话。

- `TcpService::startWorkerThread(size_t threadNum, FRAME_CALLBACK callback = nullptr)`

    (线程安全)开启工作线程，每一个工作线程有一个`EventLoop`负责事件检测。


- `TcpService::addDataSocket(sock fd, SSLHelper::PTR ssl, bool isUseSSL, ENTER_CALLBACK, DISCONNECT_CALLBACK, DATA_CALLBACK, size_t maxRecvBuffer, bool forceSameLoop)`

    (线程安全)此函数是最负责的函数，用于添加socket到管理对象中。</br>当底层绑定成功会调用`ENTER_CALLBACK`，当链接断开会调用`DISCONNECT_CALLBACK`，当收到数据则会调用`DATA_CALLBACK`。</br>
    这三个回调的第一个参数就标识网络会话对象，其类型是`int64_t`。</br>
    `forceSameLoop`参数表示是否绑定到当前线程所属的`EventLoop`中。</br>
    需要特别提醒的是：`isUseSSL`参数标识是否使用SSL，当它为`true`时，如果`ssl`为`nullptr`则表示当前在`server side`，也就是说当我们编写服务器端代码时，如果要使用SSl，那么`ssl`不应该为`nullptr`。如果是编写的客户端，那么必须为`nullptr`。

- `TcpService::send(int64_t id, std::shared_ptr<string> msg, PACKED_SENDED_CALLBACK cb)`

    (线程安全)向id标识的网络会话发送消息，当发送完成会调用`cb`

- `TcpService::postShutdown(int64_t id)`

    (线程安全)异步执行对id标识的网络会话的postShutdown，关闭写。

- `TcpService::postDisConnect(int64_t id)`

    (线程安全)异步断开id标识的网络会话的，会触发`addDataSocket`接口中传入的`DISCONNECT_CALLBACK`。


## 示例
```C++
auto service = TcpService::Create();
// use blocking connect for test
auto fd = ox_socket_connect(false, ip, port);
service->addDataSocket(fd,
    nullptr,
    false,
    [](int64_t id) {
        std::cout << "client enter, id is:" << id << std::endl;
    },
    [](int64_t id) {
        std::cout << "client close, id is:" << id << std::endl;
    },
    [service](int64_t id, const char* buffer, size_t len) {
        std::cout << "recv data from client, id is:" << id << std::endl;
        service->send(id, buffer, len); // echo
        return len;
    },
    1024,
    false);

std::this_thread::sleep_for(2s);
```

# 提示
- 在任何线程任何时候使用`TcpService`对象操作某个网络会话`int64_t id`时都是安全的，哪怕A线程执行了`postDisConnect`，B线程再调用`send`，那也是安全的。

# 注意事项
- 通常在使用`brynet`时不会使用`TcpService`，因为没有接口和方式能够设置网络会话对象的`user data`，</br>但我们应用程序开发总是会遇到将网络会话关联到某个逻辑会话的需求。如果使用`TcpService`，需要用户自己管理这个映射/绑定关系（比如使用线程安全的`map<int64_t, Player>`)。</br>因此，一般而言我们推荐使用[wrap_tcp_service](https://github.com/IronsDu/brynet/blob/master/docs/wrap_tcp_service.zh-cn.md)，它提供的`TCPSession`可以直接设置`user data`绑定逻辑层对象。
