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

# 关于效率
   * 吞吐测试：CentOS7虚拟机(Win10 Host,i5-6500四核,8G内存)(虚拟机设置为4核CPU,2G内存)下进行PingPong测试(localhost)：
   
      *  1000链接，消息大小4k，吞吐410M/s
      *  10000链接，消息大小4k，吞吐305M/s

   * 千兆局域网下延迟和吞吐测试，服务器采用Win 2008（Xeon E3四核，4G内存），客户端采用Win10 进行"PingPong"测试：
      说明：消息采用protobuf格式,每个链接上跑100个消息(每个pb消息49字节)(网络占用100%, 500Mbps)

      *  10链接，60W QPS/s，延迟不大于5ms，80%延迟在1ms(逻辑数据29M/s)
      *  100链接，81W QPS/s, 延迟不大于25ms，平均延迟为9ms(逻辑数据37M/s)

   * CentOS7虚拟机开启360链接到服务器(localhost)，进行广播测试(收到一个消息包后转发给所有客户端)
   
      *  每个消息包带有客户端标识，当客户端收到标识为自己的消息包后，继续发送消息包给服务器(类似pingpong)
      *  转发量 220W+/s

   * 在所有测试中，linux下效率均比windows下高出不少

# 使用案例：
   * [DBproxy redis代理服务器](https://github.com/IronsDu/DBProxy)
   * [DServerFramework 分布式游戏服务器框架](https://github.com/IronsDu/DServerFramework)
   * [Joynet Lua网络库](https://github.com/IronsDu/Joynet)
