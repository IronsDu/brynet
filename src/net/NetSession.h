#ifndef _NETSESSION_H
#define _NETSESSION_H

#include <string>
#include "WrapTCPService.h"

using namespace brynet::net;

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
        mSocketID = 0;
    }

    void    setSession(const WrapTcpService::PTR& service, int64_t socketID, const std::string& ip)
    {
        mService = service;
        mSocketID = socketID;
        mIP = ip;
    }

    const WrapTcpService::PTR& getService()
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
        return mSocketID;
    }

    void            postClose()
    {
        mService->getService()->disConnect(mSocketID);
    }

    void            sendPacket(const char* data, size_t len, DataSocket::PACKED_SENDED_CALLBACK callback = nullptr)
    {
        mService->getService()->send(mSocketID, DataSocket::makePacket(data, len), std::move(callback));
    }

    void            sendPacket(DataSocket::PACKET_PTR packet, DataSocket::PACKED_SENDED_CALLBACK callback = nullptr)
    {
        mService->getService()->send(mSocketID, std::move(packet), std::move(callback));
    }

    auto      getEventLoop()
    {
        return mService->getService()->getEventLoopBySocketID(mSocketID);
    }

private:
    std::string         mIP;
    WrapTcpService::PTR mService;
    int64_t             mSocketID;
};

void WrapAddNetSession(WrapTcpService::PTR service, sock fd, BaseNetSession::PTR pClient, int pingCheckTime, size_t maxRecvBufferSize);

#endif