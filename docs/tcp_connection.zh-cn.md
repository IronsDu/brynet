# 概述
`TcpConnection::Ptr`是对TCP连接的封装.源代码见:[TcpConnection.h](https://github.com/IronsDu/brynet/blob/master/include/brynet/net/TcpConnection.hpp).

# 接口

- `TcpConnection::Create(TcpSocket::Ptr, size_t maxRecvBufferSize, EnterCallback&&, EventLoop::Ptr)`
    

    必须(只能)使用此静态方法创建`TcpConnection::Ptr`智能指针对象,用于后续工作.`maxRecvBufferSize`表示最大接收缓冲区（它一定要不小于逻辑层单个最大包的长度！）。

- `TcpConnection::send(const char* buffer, size_t len, PacketSendedCallback&& callback = nullptr)`
    
    发送消息.如果`callback`不为nullptr，那么当io线程将此消息全部"发送完成"(投递到内核TCP缓冲区)时会调用callback。

- `TcpConnection::send(const SendableMsg::Ptr& msg, PacketSendedCallback&& callback)`
    
	发送消息，用户可以继承SendableMsg，重写`data`和`size`函数，且最重要的是：能实现自己的消息内存管理器。
		
- `Connection::setDataCallback(std::function<void(brynet::base::BasePacketReader&)> callback)`

    设置处理接受消息的回调函数。等下次接收到对端发送的数据时，会再传递给此回调函数。
