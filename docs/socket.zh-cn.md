# 概述
`TcpSocket::PTR`和`ListenSocket::PTR`是对socket fd的封装，结合`std::unique_ptr`使得fd资源更安全，避免泄露.源代码见:[Socket.h](https://github.com/IronsDu/brynet/blob/master/src/brynet/net/Socket.h).

# 接口

- `TcpSocket::Create`
    

    必须(只能)使用此静态方法创建`TcpSocket::PTR`智能指针对象,用于后续工作.

- `TcpSocket::isServerSide(void)`
    
    检测`socket`是否为服务端server（即为accept返回的），如果为false，则表示是客户端socket。

- `TcpSocket::SocketNodelay(void)`

    设置`TCP_NODELAY`

- `TcpSocket::SocketNonblock(void)`

    设置非阻塞

- `TcpSocket::SetSendSize(int size)`

    设置`SO_SNDBUF`发送缓冲区大小(通常不建议修改)

- `TcpSocket::SetRecvSize(int size)`

    设置`SO_RCVBUF`发送缓冲区大小(通常不建议修改)

- `TcpSocket::GetIP(void)`

    获取socket的远端IP地址


## 示例
无。因为通常`brynet`的使用者不需要自己通过fd来创建`TcpSocket`对象，而是直接使用`Connector`和`ListenThread`所产生的`TcpSocket::PTR`即可.
