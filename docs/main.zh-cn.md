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
  详情见[listen_thread](https://github.com/IronsDu/brynet/blob/master/docs/listen_thread.zh-cn.md)和[connector](https://github.com/IronsDu/brynet/blob/master/docs/connector.zh-cn.md)

## 安全的Socket对象
`brynet`不对用户暴露原始的socket fd，而是提供`TcpSocket::PTR`，详见[Socket](https://github.com/IronsDu/brynet/blob/master/docs/socket.zh-cn.md)

## 数据传输
分别(可以)由以下几个方式进行提供数据读写

- 使用裸指针表示网络对话对象,需要用户手动配合使用`EventLoop`.
  详情见[datasocket](https://github.com/IronsDu/brynet/blob/master/docs/datasocket.zh-cn.md)
- 使用`int64_t`表示网络对话对象,提供线程安全的发送接口,无需用户接触`EventLoop`,安全方便.
  详情见[tcp_service](https://github.com/IronsDu/brynet/blob/master/docs/tcp_service.zh-cn.md)
- 通过封装`TCPService`,使用`TCPSession::PTR`表示网络对象.
  详情见[wrap_tcp_service](https://github.com/IronsDu/brynet/blob/master/docs/wrap_tcp_service.zh-cn.md)

## 高级特性
- 提供 `promise receive` 方便解析网络消息，详见:[promise_receive](https://github.com/IronsDu/brynet/blob/master/docs/promise_receive.zh-cn.md)

# 效率优化
  详情见[merge_send](https://github.com/IronsDu/brynet/blob/master/docs/merge_send.zh-cn.md)

# 使用SSL
请确保你已经安装了openssl，然后在编译`brynet`时定义`USE_OPENSSL`宏即可。

# 完整示例
请查看[example](https://github.com/IronsDu/brynet/tree/master/examples)

# 应用项目
* [Redis proxy](https://github.com/IronsDu/DBProxy)
* [Distributed game server framework](https://github.com/IronsDu/DServerFramework)
* [Joynet - Lua network library](https://github.com/IronsDu/Joynet)
* [HTTP-RPC](https://github.com/IronsDu/http-rpc)
* [grpc-gateway](https://github.com/IronsDu/grpc-gateway)