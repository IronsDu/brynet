# 概述
`brynet` 是一个多线程的异步网络库，能够运行在Linux和Windows环境下。
仅仅需要C++ 11编译器，且没有其他任何第三方依赖。

# 架构
`brynet` 核心分为以下几个部分
## 事件层
  由`EventLoop`提供,用于检测socket的可读可写,并包含一个事件通知,可用于线程间通信(消息队列).
  详情见[eventloop](https://github.com/IronsDu/brynet/blob/master/docs/eventloop.zh-cn.md)

## 网络连接
  由`ListenThread`和`AsyncConnector`提供,前者用于接收外部的链接请求,后者用于向外部进行网络连接.
  详情见[listen_thread](https://github.com/IronsDu/brynet/blob/master/docs/listen_thread.zh-cn.md)和[connector](https://github.com/IronsDu/brynet/blob/master/docs/connector.zh-cn.md)，使用这两个组件可以让我们得到`TcpSocket::Ptr`和`TcpConnection::Ptr`，`TcpConnection::Ptr`是我们最会频繁使用的类型。

## 安全的Socket对象
`brynet`不对用户暴露原始的socket fd，而是提供`TcpSocket::Ptr`，详见[Socket](https://github.com/IronsDu/brynet/blob/master/docs/socket.zh-cn.md)

## 安全的TcpConnection连接对象
  当然用户也一般不直接使用`TcpSocket::Ptr`，而是使用`TcpConnection::Ptr`，详见[TcpConnection](https://github.com/IronsDu/brynet/blob/master/docs/tcp_connection.zh-cn.md)

## 高级特性
- 提供 `promise receive` 方便解析网络消息，详见:[promise_receive](https://github.com/IronsDu/brynet/blob/master/docs/promise_receive.zh-cn.md)

# 效率优化
  详情见[merge_send](https://github.com/IronsDu/brynet/blob/master/docs/merge_send.zh-cn.md)

# 使用SSL
请确保你已经安装了openssl，然后在编译`brynet`时定义`USE_OPENSSL`宏即可。

# 完整示例
请查看[example](https://github.com/IronsDu/brynet/tree/master/examples)

# 建议
- 建议使用[wrapper](https://github.com/IronsDu/brynet/blob/master/include/brynet/net/wrapper)目录下的包装代码来使用brynet，它会让您使用得更便捷，提高您的开发效率。在[examples](https://github.com/IronsDu/brynet/tree/master/examples)目录里有一些示例就使用了它，您可以参考一下。
- 你完全可以使用(链接)官方的http_parser，将其路径添加到工程的包含路径即可（则会自动替代brynet自带的http_parser.h)

# 应用项目
* [ARK - distributed plugin framework](https://github.com/ArkNX/ARK)
* [Redis proxy](https://github.com/IronsDu/DBProxy)
* [gayrpc](https://github.com/IronsDu/gayrpc)
* [gaylord - distributed virtual actor framework](https://github.com/IronsDu/gaylord)
* [Joynet - Lua network library](https://github.com/IronsDu/Joynet)
* [grpc-gateway](https://github.com/IronsDu/grpc-gateway)