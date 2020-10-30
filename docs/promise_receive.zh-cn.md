# 概述
`PromiseReceive`可以提供更高级更方便的消息解析.源代码见:[PromiseReceive.h](https://github.com/IronsDu/brynet/blob/master/include/brynet/net/PromiseReceive.hpp).</br>
使用此辅助类，可以更方便的解析`HTTP`、`Redis`等文本协议

# 接口

- `brynet::net::setupPromiseReceive(const TCPSession::Ptr& session)`
  
首先使用此静态方法创建`PromiseReceive::Ptr`智能指针对象,接管 `session`的数据解析.</br>
    因此使用者一定不要再为`TCPSession::Ptr`调用`setDataCallback`接口设置数据回调！
    
- `PromiseReceive::receive(size_t len, Handle handle)`
  
    (非线程安全)投递一个异步读取，当满足长度>=len时，调用`handle`</br>
    `Handle`类型为：`std::function<bool(const char* buffer, size_t len)>`，当`handle`返回值为`true`表示重复投递此`receive`请求（待下次满足条件时会继续调用`handle`)

- `PromiseReceive::receive(std::shared_ptr<size_t> len, Handle handle)`
  
    (非线程安全)重载版本，提供智能指针作为参数，此值delay决议，用于调用`receive`时不确定长度的情况。

- `PromiseReceive::receiveUntil(std::string str, Handle handle)`
  
    (非线程安全)当收到的数据中包含`str`时，回调`handle`。

## 示例
```C++
auto enterCallback = [](const TcpConnection::Ptr& session) {
        auto promiseReceive = setupPromiseReceive(session);
        auto contentLength = std::make_shared<size_t>();

        promiseReceive
            ->receiveUntil("\r\n", [](const char* buffer, size_t len) {
                auto headline = std::string(buffer, len);
                std::cout << headline << std::endl;
                return false;
            })
            ->receiveUntil("\r\n", [promiseReceive, contentLength](const char* buffer, size_t len) {
                auto headerValue = std::string(buffer, len);
                std::cout << headerValue << std::endl;
                if (len > 2)
                {
                    const static std::string ContentLenghtFlag = "Content-Length: ";
                    auto pos = headerValue.find(ContentLenghtFlag);
                    if (pos != std::string::npos)
                    {
                        auto lenStr = headerValue.substr(pos + ContentLenghtFlag.size(), headerValue.size());
                        *contentLength = std::stoi(lenStr);
                    }
                    return true;
                }
                return false;
            })->receive(contentLength, [session](const char* buffer, size_t len) {
                HttpResponse response;
                response.setStatus(HttpResponse::HTTP_RESPONSE_STATUS::OK);
                response.setContentType("text/html; charset=utf-8");
                response.setBody("<html>hello world </html>");

                auto result = response.getResult();
                session->send(result.c_str(), result.size());
                session->postShutdown();

                return false;
            });
    };
```

# 注意事项
- `PromiseReceive`的接口都不是线程安全的，请务必小心，确保只在一个线程中调用相关接口。
- 务必注意`receive`和`receiveUntil`中的回调参数的返回值(false/true)。