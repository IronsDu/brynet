# 概述
`AsyncConnector`是一个异步创建外部连接的类.源代码见:[AsyncConnector.hpp](https://github.com/IronsDu/brynet/blob/master/include/brynet/net/AsyncConnector.hpp).

# 接口

- `AsyncConnector::Create`
  

    必须(只能)使用此静态方法创建`AsyncConnector::Ptr`智能指针对象,以用于后续使用.

- `AsyncConnector::startWorkerThread(void)`
  
    (线程安全)使用此成员函数即可开启工作线程(在开启的工作线程里负责异步链接)

- `AsyncConnector::stopWorkerThread(void)`
  
    (线程安全)停止工作线程(当工作线程结束时此函数才返回)

- `AsyncConnector::asyncConnect(const std::vector<ConnectOption::ConnectOptionFunc>& options)`
  
    (线程安全)请求创建外部链接,options为异步连接的选项,比如`ConnectOption::WithAddr`为指定服务器地址,`ConnectOption::WithCompletedCallback`则指定完成回调</br>
    如果没有开启工作线程,那么此函数会产生异常!

## 示例
```C++
auto connector = AsyncConnector::Create();
connector->startWorkerThread();

// set timeout is 1s
connector->asyncConnect({
        ConnectOption::WithAddr("127.0.0.1", 9999),
        ConnectOption::WithTimeout(std::chrono::seconds(1)),
        ConnectOption::WithCompletedCallback([](TcpSocket::Ptr socket) {
            std::cout << "connect success" << std::endl;
            // 在此我们就可以将 socket 用于网络库的其他部分,比如用于`TCPService::addTcpConnection`
        }),
        ConnectOption::WithFailedCallback([]() {
            std::cout << "connect failed" << std::endl;
        })
    });

// wait 2s for completed
std::this_thread::sleep_for(2s);

connector->stopWorkerThread();
```

# 注意事项
- 如果没有开启工作线程或者关闭了工作线程,那么 `asyncConnect` 接口中传入的两个回调均不会被执行,并且会产生异常.
  因此确保不要随意在服务工作时调用`stopWorkerThread`
- 如果给`asyncConnect` 接口传入的选项不同时包含`WithCompletedCallback`和`WithFailedCallback`,会产生异常,因为我们不可能使用asyncConnect时
既不关心成功也不关心失败。
