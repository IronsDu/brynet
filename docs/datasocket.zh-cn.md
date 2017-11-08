# 概述
`DataSocket`是`brynet`里最基本最核心的网络会话对象,负责实际的I/O读写。
源代码见:[DataSocket.h](https://github.com/IronsDu/brynet/blob/master/src/brynet/net/DataSocket.h).

# 接口

- `DataSocket::DataSocket(sock fd, size_t maxRecvBufferSize)`
    

    创建网络会话对象，且只能使用指针。</br>
    参数 `sock fd`通常产生自`ListenThread`[doc](https://github.com/IronsDu/brynet/blob/master/docs/listen_thread.zh-cn.md#接口)和`AsyncConnector`[doc](https://github.com/IronsDu/brynet/blob/master/docs/connector.zh-cn.md#接口)。

- `DataSocket::onEnterEventLoop(const EventLoop::PTR&)`
    
    将网络会话对象和`EventLoop`进行绑定/注册，也即使用后者负责此网络会话的可读写检测。</br>
    此函数只能在`EventLoop::loop`所在线程调用。

- `DataSocket::setEnterCallback(std::function<void(PTR)> cb)`
    
    当`onEnterEventLoop`执行成功时会执行`cb`

- `DataSocket::setDataCallback(std::function<size_t(PTR, const char* buffer, size_t len)> cb)`
    
    当收到数据时会执行`cb`，`cb`的返回值表示其处理了多少字节数(n <= len)。</br>
    比如在游戏通信中的unpack处理中，如果返回值<n，则表示还有剩余数据（非完整包），底层会将剩余数据缓存，待下次收到数据时继续交给`cb`。

- `DataSocket::setDisConnectCallback(std::function<void(PTR)> cb)`

    当会话对象断开时调用`cb`，一个对话有且只会触发一次`cb`。

- `DataSocket::send(const char* buffer, size_t len, const PACKED_SENDED_CALLBACK& cb = nullptr)`

    (线程安全)此函数用于异步发送消息，它是`brynet`中调用最频繁的函数。</br>
    当底层实际调用了`::send`系统调用将`buffer`完全压入了内核发送缓冲区时，（如果cb不为nullptr）会回调`cb`。

- `DataSocket::send(std::shared_ptr<std::string>, const PACKED_SENDED_CALLBACK& cb = nullptr)`

    (线程安全)此重载函数用于发送数据，数据由智能指针提供。

- `DataSocket::setHeartBeat(std::chrono::nanoseconds checkTime)`

   (线程安全) 设置keepalive检测，当 `checkTime`内没有收到对方的数据时，会主动断开连接。</br>
    当`checkTime`的值为`std::chrono::nanoseconds::zero`时则停止检测。

- `DataSocket::postDisConnect(void)`

    (线程安全)投递主动断开，当投递此函数断开后，仍然会触发`setDisConnectCallback`中设置的回调。</br>
    因此使用者尽量保证在统一的地方（比如断开回调用）进行断开业务逻辑的处理，避免重复处理。

- `DataSocket::postShutdown(void)`

    (线程安全)投递shutdown（不会直接触发任何回调）（当然最终由对方导致的断开，则会触发断开回调）

- `DataSocket::setUD(BrynetAny value`

    (线程安全)设置应用层user data

## 示例
```C++
auto ev = std::make_shared<EventLoop>();
while(true)
{
    ev->loop(1)

    auto fd = ox_socket_connect(false, ip, port);

    auto datasocket = new DataSocket(fd, 10000);
    datasocket->setEnterCallback([](DataSocket::PTR datasocket) {
    });
    datasocket->setDisConnectCallback([](DataSocket::PTR datasocket) {
        delete datasSocket;
        // 务必确保没有其他地方使用datasSocket
    });
    datasocket->setDataCallback([](DataSocket::PTR datasSocket, const char* buffer, size_t len) {
        // 你完全允许在这里调用 datasSocket->postShutdown();
        // 也可以在这里调用 datasSocket->send(buffer,len); 实现echo服务
        return len;
    });

    // 设置存活时间，如果距离上次接收数据的时间达到1s则会断开链接
    datasocket->setHeartBeat(std::chrono::seconds(1));

    // 这里只是演示多线程环境下怎么调用onEnterEventLoop
    // 所以使用了pushAsyncProc。
    ev->pushAsyncProc([ev, datasocket]() {
        if (!datasocket->onEnterEventLoop(ev)) {
            // 如果绑定失败则应该删除DataSocket，因为不可能也不应该在其他地方再使用它
            delete datasocket;
        }
    });
}
```

# 注意事项
- `brynet`不建议直接使用`DataSocket`，因为它不止需要用户自己配合`EventLoop`，而且它是裸指针，十分不安全（特别是涉及多线程的时候）。</br>
当你不小心释放了它，却在其他地方/线程继续使用时你的服务会`crash`。
- 通常应该在构造`DataSocket`之后，在`onEnterEventLoop`调用之前进行设置各个事件回调，且在此之后不要再进行重设。`DataSocket`中的接口也只有这三个回调设置不是线程安全的。
