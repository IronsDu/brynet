#ifndef _WRAP_TCP_SERVICE_H
#define _WRAP_TCP_SERVICE_H

#include <string>
#include <stdint.h>

#include "TCPService.h"

class WrapServer;

class TCPSession
{
public:
    typedef std::shared_ptr<TCPSession>     PTR;
    typedef std::weak_ptr<TCPSession>       WEAK_PTR;

    typedef std::function<void(TCPSession::PTR)>   CLOSE_CALLBACK;
    typedef std::function<int(TCPSession::PTR, const char*, int)>   DATA_CALLBACK;

    TCPSession();

    virtual ~TCPSession();
    int64_t                 getUD();
    void                    setUD(int64_t ud);

    std::string             getIP() const;
    int64_t                 getSocketID() const;

    void                    send(const char* buffer, int len, const DataSocket::PACKED_SENDED_CALLBACK& callback = nullptr);
    void                    send(const DataSocket::PACKET_PTR& packet, const DataSocket::PACKED_SENDED_CALLBACK& callback = nullptr);

    void                    postClose();
    void                    setCloseCallback(const CLOSE_CALLBACK& callback);

    void                    setDataCallback(const DATA_CALLBACK& callback);

private:
    void                    setSocketID(int64_t id);
    void                    setIP(const std::string& ip);

    void                    setService(TcpService::PTR service);

    CLOSE_CALLBACK&         getCloseCallback();

    DATA_CALLBACK&          getDataCallback();

private:
    TcpService::PTR         mService;
    int64_t                 mSocketID;
    std::string             mIP;
    int64_t                 mUserData;

    CLOSE_CALLBACK          mCloseCallback;
    DATA_CALLBACK           mDataCallback;

    friend class WrapServer;
};

class WrapServer
{
public:
    typedef std::shared_ptr<WrapServer> PTR;
    typedef std::weak_ptr<WrapServer>   WEAK_PTR;

    typedef std::function<void(TCPSession::PTR)>   SESSION_ENTER_CALLBACK;

    WrapServer();
    virtual ~WrapServer();

    TcpService::PTR&        getService();

    void                    startWorkThread(int threadNum, TcpService::FRAME_CALLBACK callback = nullptr);

    void                    addSession(int fd, SESSION_ENTER_CALLBACK userEnterCallback, bool isUseSSL, int maxRecvBufferSize);
private:
    TcpService::PTR         mTCPService;
};

#endif