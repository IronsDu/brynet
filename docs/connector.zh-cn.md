# 概述
`AsyncConnector`是一个异步创建外部连接的类.源代码见:[Connector.h](https://github.com/IronsDu/brynet/blob/master/src/brynet/net/Connector.h).

# 接口

- `AsyncConnector::Create`
    

    必须(只能)使用此静态方法创建`AsyncConnector::PTR`智能指针对象,用于后续工作.

- `AsyncConnector::startWorkerThread(void)`
    
    (线程安全)使用此成员函数即可开启工作线程(在其中负责异步链接的处理)

- `AsyncConnector::stopWorkerThread(void)`
    
    (线程安全)停止工作线程(当工作线程结束时函数返回)

- `AsyncConnector::asyncConnect(const std::string& ip, int port, std::chrono::nanoseconds timeout, COMPLETED_CALLBACK, FAILED_CALLBACK)`
    
    (线程安全)请求创建外部链接.当创建成功或失败后会调用对应的回调函数，也即如果链接成功则调用`COMPLETED_CALLBACK`，否则调用`FAILED_CALLBACK`。</br>
    如果没有开启工作线程,那么此函数会产生异常!

## 示例
```C++
auto connector = AsyncConnector::Create();
connector->startWorkerThread();

// set timeout is 1s
connector->asyncConnect("127.0.0.1", 
    9999, 
    std::chrono::seconds(1),
    [](TcpSocket::PTR socket) {
        std::cout << "connect success" << std::endl;
        // 在此我们就可以将 fd 用于网络库的其他部分,比如用于`TCPService`
    },
    []() {
        std::cout << "connect failed" << std::endl;
    });

// wait 2s for completed
std::this_thread::sleep_for(2s);

connector->stopWorkerThread();
```

# 注意事项
- 如果没有开启工作线程或者关闭了工作线程,那么 `asyncConnect` 接口中传入的两个回调均不会被执行,并会产生异常.
  因此确保不要随意在服务工作时调用`stopWorkerThread`
- 如果`asyncConnect` 接口中传入的两个回调均为nullptr,会产生异常
