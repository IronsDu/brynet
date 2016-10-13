#ifndef _WRAP_TCP_SERVICE_H
#define _WRAP_TCP_SERVICE_H

#include <string>
#include <stdint.h>

#include "TCPService.h"
#include "NonCopyable.h"

class WrapServer;

/*  以智能指针对象为网络会话标识的网络服务   */
class TCPSession : public NonCopyable
{
public:
    typedef std::shared_ptr<TCPSession>     PTR;
    typedef std::weak_ptr<TCPSession>       WEAK_PTR;

    typedef std::function<void(TCPSession::PTR&)>   CLOSE_CALLBACK;
    typedef std::function<size_t(TCPSession::PTR&, const char*, size_t)>   DATA_CALLBACK;

    int64_t                 getUD() const;
    void                    setUD(int64_t ud);

    const std::string&      getIP() const;
    int64_t                 getSocketID() const;

    void                    send(const char* buffer, size_t len, const DataSocket::PACKED_SENDED_CALLBACK& callback = nullptr) const;
    void                    send(const DataSocket::PACKET_PTR& packet, const DataSocket::PACKED_SENDED_CALLBACK& callback = nullptr) const;

    void                    postShutdown() const;
    void                    postClose() const;

    void                    setCloseCallback(CLOSE_CALLBACK&& callback);
    void                    setCloseCallback(const CLOSE_CALLBACK& callback);

    void                    setDataCallback(DATA_CALLBACK&& callback);
    void                    setDataCallback(const DATA_CALLBACK& callback);

protected:
    ~TCPSession();

private:
    TCPSession();

    void                    setSocketID(int64_t id);
    void                    setIP(const std::string& ip);

    void                    setService(TcpService::PTR& service);

    CLOSE_CALLBACK&         getCloseCallback();

    DATA_CALLBACK&          getDataCallback();

    static  PTR             Create()
    {
        struct make_shared_enabler : public TCPSession {};
        return std::make_shared<make_shared_enabler>();
    }

private:
    TcpService::PTR         mService;
    int64_t                 mSocketID;
    std::string             mIP;
    int64_t                 mUserData;

    CLOSE_CALLBACK          mCloseCallback;
    DATA_CALLBACK           mDataCallback;

    friend class WrapServer;
};

class WrapServer : public NonCopyable
{
public:
    typedef std::shared_ptr<WrapServer> PTR;
    typedef std::weak_ptr<WrapServer>   WEAK_PTR;

    typedef std::function<void(TCPSession::PTR&)>   SESSION_ENTER_CALLBACK;

    WrapServer();
    virtual ~WrapServer();

    TcpService::PTR&        getService();

    void                    startWorkThread(size_t threadNum, TcpService::FRAME_CALLBACK callback = nullptr);

    void                    addSession(sock fd, const SESSION_ENTER_CALLBACK& userEnterCallback, bool isUseSSL, int maxRecvBufferSize, bool forceSameThreadLoop = false);

private:
    TcpService::PTR         mTCPService;
};

#endif