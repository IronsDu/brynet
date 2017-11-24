# 概述
`WrapTcpService`是对`TcpService`的封装，源代码见：[WrapTCPService.h](https://github.com/IronsDu/brynet/blob/master/src/brynet/net/WrapTCPService.h).</br>
它提供一个额外的`TCPSession`类型作为网络会话对象，比起`TcpService`以`int64_t`作为会话ID功能更强大。

# WrapTcpService 接口

- `WrapTcpService::WrapTcpService()`

    构造`WrapTcpService`管理器对象，可以是智能指针，也可以是值对象（但禁止拷贝，不建议用）。

- `WrapTcpService::startWorkThread(size_t threadNum, FRAME_CALLBACK  cb)`

    (线程安全)开启工作线程，在每个工作线程中的`EventLoop::loop`调用之后会回调一次`cb`(如果`cb`不为nullptr)。

- `WrapTcpService::addSession(sock fd, SESSION_ENTER_CALLBACK cb, bool isUseSSL, SSLHelper::PTR, size_t maxRecvBufferSize, bool forceSameThreadLoop)`

    (线程安全)此函数类似`TcpService::addDataSocket`，区别在于 `cb` 回调的类型为:`std::function<void(const TCPSession::PTR&)`。</br>
    它的`TCPSession::PTR`参数即为创建的网络会话对象，以后的工作都将使用它。</br>
    此函数与`TcpService::addDataSocket`类似，其他几个参数请参考它，再次不做重复说明。

# TCPSession (部分)接口

- `TCPSession::send(const char* buffer, size_t len, PACKED_SENDED_CALLBACK cb)`

    (线程安全)发送消息，当消息投递到系统缓冲区时，会回调`cb`。此函数类似`TcpService::send`。

- `TCPSession::postShutdown()`

    (线程安全)发异步投递shutdown(WRITE)

- `TCPSession::postDisConnect()`

    (线程安全)发投递异步断开

- `TCPSession::setUD(Any ud)`

    (非线程安全)设置/绑定用户层对象

- `TCPSession::getUD()`

     (非线程安全)获取用户层对象

- `TCPSession::setDisConnectCallback(DISCONNECT_CALLBACK cb)`

    (非线程安全)设置断开回调

- `TCPSession::setDataCallback(DATA_CALLBACK cb)`

    (非线程安全)设置数据回调，`DATA_CALLBACK `类型为`std::function<size_t(const TCPSession::PTR&, const char*, size_t)>`。

# 提示
- 建议使用`WrapTcpService`和`TCPSession`
- 只要你拥有一个`TCPSession::PTR`，那么你可以在任何线程任何时候调用`send`、`postShutdown`以及`postDisConnect`，这都是安全的，无论它是否已经发生了断开等情况。

# 示例
```C++
auto service = std::make_shared<WrapTcpService>();
// use blocking connect for test
auto fd = ox_socket_connect(false, ip, port);

service->addSession(fd,
    [](const TCPSession::PTR& session) {
        session->setUD(1);

        session->setDisConnectCallback([](const TCPSession::PTR& session) {
            auto ud = brynet::net::cast<int64_t>(session->getUD());
            std::cout << "close by" << *ud << std::endl;
        });

        session->setDataCallback([](const TCPSession::PTR& session, const char* buffer, size_t len) {
            auto ud = brynet::net::cast<int64_t>(session->getUD());
            std::cout << "recv data from " << *ud << std::endl;
            session->send(buffer, len); // echo
            return len;
        });
    },
    false,
    nullptr,
    1024,
    false);

std::this_thread::sleep_for(2s);
```

# 注意事项
- `setUD`、`setDisConnectCallback`以及`setDataCallback`都不是线程安全的。</br>
因此最好在`addSession`中的`SESSION_ENTER_CALLBACK`回调里统一设置，避免读写冲突。</br>
同样，`getUD`也不是线程安全的，但这只表示它和`setUD`会有冲突，因此当你遵守前面的建议，那么你之后无论怎么调用`getUD`都不会有问题。
- 当你采用C++11时，`UD`的类型为`int64_t`，当采用`C++14`+的时候则为`std::any`。
