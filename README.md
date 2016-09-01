##跨平台游戏开发组件
Windows : [![Build status](https://ci.appveyor.com/api/projects/status/je9n1g26yah31e5e/branch/master?svg=true)](https://ci.appveyor.com/project/IronsDu/accumulation-dev/branch/master)  Linux : [![Build Status](https://travis-ci.org/IronsDu/accumulation-dev.svg?branch=master)](https://travis-ci.org/IronsDu/accumulation-dev)

# 介绍
* src\net TCP 网络库(C++ 11 实现)
    * 特性：
      * 跨平台(Windows、Linux）、高性能(IOCP、EPOLL ET）
      * 会话安全
      * 多线程
      * 支持 SSL 通信
      * 支持 HTTP/HTTPS
      * 支持 WebSocket
* src\rpc:
    * 只提供序列化和反序列化的rpc api（支持异步回调）
* src\timer:
    * C++ 定时器(使用weak_ptr)
* src\utils:
    * 线程间消息队列
    * 线程池，协程序，C 定时器，异步Connector
* src\examples:
    * 网络库测试
    * protobuf消息的反射测试

# 使用案例：
   * [DBproxy redis代理服务器](https://github.com/IronsDu/DBProxy)
   * [DServerFramework 分布式游戏服务器框架](https://github.com/IronsDu/DServerFramework)
   * [Joynet Lua网络库](https://github.com/IronsDu/Joynet)
