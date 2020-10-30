# 概述
`TcpSocket::Ptr`和`ListenSocket::Ptr`是对socket fd的封装，结合`std::unique_ptr`使得fd资源更安全，避免泄露.源代码见:[Socket.h](https://github.com/IronsDu/brynet/blob/master/include/brynet/net/Socket.hpp).

# 接口

- `TcpSocket::Create(sock fd, bool serverSide)`
  
必须(只能)使用此静态方法创建`TcpSocket::Ptr`智能指针对象,用于后续工作.`serverSide`表示是否服务端socket，如果是accept返回的fd，那么`serverSide`则为true，
    如果是`connect`返回的fd，则为false。
    
- `TcpSocket::isServerSide(void)`
  
    检测`socket`是否为服务端server（即为accept返回的），如果为false，则表示是客户端socket（即为connect返回的fd)。

- `TcpSocket::setNodelay(void)`

    设置`TCP_NODELAY`

- `TcpSocket::setNonblock(void)`

    设置非阻塞

- `TcpSocket::setSendSize(int size)`

    设置`SO_SNDBUF`发送缓冲区大小(通常不建议修改)

- `TcpSocket::setRecvSize(int size)`

    设置`SO_RCVBUF`发送缓冲区大小(通常不建议修改)

- `TcpSocket::getRemoteIP(void)`

    获取socket的远端IP地址


## 示例
无。因为通常`brynet`的使用者不需要自己通过fd来创建`TcpSocket`对象，而是直接使用`Connector`和`ListenThread`所产生的`TcpSocket::Ptr`即可.
