#ifndef _NETSESSION_H
#define _NETSESSION_H

#include <string>
#include <brynet/net/WrapTCPService.h>

/*应用服务器的网络层会话对象基类*/
class BaseNetSession
{
public:
    typedef std::shared_ptr<BaseNetSession>  PTR;
    typedef std::weak_ptr<BaseNetSession>   WEAK_PTR;

    BaseNetSession()
    {
        mService = nullptr;
    }

    virtual ~BaseNetSession()
    {
    }

    void    setSession(const brynet::net::WrapTcpService::PTR& service, const brynet::net::TCPSession::PTR& session, const std::string& ip)
    {
        mService = service;
        mSession = session;
        mIP = ip;
    }

    const brynet::net::WrapTcpService::PTR& getService()
    {
        return mService;
    }

    /*处理收到的数据*/
    virtual size_t  onMsg(const char* buffer, size_t len) = 0;
    /*链接建立*/
    virtual void    onEnter() = 0;
    /*链接断开*/
    virtual void    onClose() = 0;

    const std::string&  getIP() const
    {
        return mIP;
    }

    auto         getSocketID() const
    {
        return mSession->getSocketID();
    }

    void            postClose()
    {
        mSession->postClose();
    }

    void            sendPacket(const char* data, size_t len, brynet::net::DataSocket::PACKED_SENDED_CALLBACK callback = nullptr)
    {
        mSession->send(brynet::net::DataSocket::makePacket(data, len), std::move(callback));
    }

    void            sendPacket(brynet::net::DataSocket::PACKET_PTR packet, brynet::net::DataSocket::PACKED_SENDED_CALLBACK callback = nullptr)
    {
        mSession->send(std::move(packet), std::move(callback));
    }

    const auto&      getEventLoop()
    {
        return mSession->getEventLoop();
    }

private:
    std::string         mIP;
    brynet::net::WrapTcpService::PTR mService;
    brynet::net::TCPSession::PTR     mSession;
};

void WrapAddNetSession(brynet::net::WrapTcpService::PTR service, sock fd, BaseNetSession::PTR pClient, int pingCheckTime, size_t maxRecvBufferSize);

#endif