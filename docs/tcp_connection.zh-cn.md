# 概述
`TcpConnection::Ptr`是对TCP连接的封装.源代码见:[TcpConnection.h](https://github.com/IronsDu/brynet/blob/master/src/brynet/net/TcpConnection.h).

# 接口

- `TcpConnection::Create(TcpSocket::Ptr, size_t maxRecvBufferSize, EnterCallback&&, EventLoop::Ptr)`
    

    必须(只能)使用此静态方法创建`TcpConnection::Ptr`智能指针对象,用于后续工作.`maxRecvBufferSize`表示最大接收缓冲区（它一定要不小于逻辑层单个最大包的长度！）。

- `TcpConnection::send(const char* buffer, size_t len, PacketSendedCallback&& callback = nullptr)`
    
    发送消息.如果`callback`不为nullptr，那么当io线程将此消息全部"发送完成"(投递到内核TCP缓冲区)时会调用callback。

- `TcpConnection::setDataCallback(std::function<size_t(const char* buffer, size_t len)> callback)`

    设置处理接受消息的回调函数。此函数的返回值表示业务层此次回调里消耗了多少字节的数据，此返回值<= len，如果返回值小于len,那么网络层会把buffer里剩余部分缓存起来，
    等下次接收到对端发送的数据时，会再一并传递给此回调函数。
