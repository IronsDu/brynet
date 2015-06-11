#ifndef _WRAP_TCP_SERVICE_H
#define _WRAP_TCP_SERVICE_H

#include "TCPService.h"

class WrapServer;

class TCPSession
{
public:
    typedef std::shared_ptr<TCPSession>    PTR;
    typedef std::function<void(TCPSession::PTR)>   CLOSE_CALLBACK;
    typedef std::function<int(TCPSession::PTR, const char*, int)>   DATA_CALLBACK;

    TCPSession();

    virtual ~TCPSession();
    int64_t                 getUD();
    void                    setUD(int64_t ud);

    void                    send(const char* buffer, int len);

    void                    setCloseCallback(const CLOSE_CALLBACK& callback);

    void                    setDataCallback(const DATA_CALLBACK& callback);

private:
    void                    setSocketID(int64_t id);

    void                    setService(TcpService::PTR service);

    CLOSE_CALLBACK&         getCloseCallback();

    DATA_CALLBACK&          getDataCallback();

private:
    TcpService::PTR         mService;
    int64_t                 mSocketID;
    int64_t                 mUserData;

    CLOSE_CALLBACK          mCloseCallback;
    DATA_CALLBACK           mDataCallback;

    friend class WrapServer;
};

class WrapServer
{
public:
    typedef std::shared_ptr<WrapServer> PTR;
    typedef std::function<void(TCPSession::PTR)>   SESSION_ENTER_CALLBACK;

    WrapServer();
    virtual ~WrapServer();

    void                    setDefaultEnterCallback(SESSION_ENTER_CALLBACK callback);

    TcpService::PTR         getService();

    void                    startListen(int port, const char *certificate = nullptr, const char *privatekey = nullptr);

    void                    startWorkThread(int threadNum, TcpService::FRAME_CALLBACK callback = nullptr);

    void                    addSession(int fd, SESSION_ENTER_CALLBACK userEnterCallback);
private:
    void                    onAccept(int fd);

private:
    TcpService::PTR         mTCPService;
    ListenThread            mListenThread;
    SESSION_ENTER_CALLBACK  mSessionDefaultEnterCallback;
};

#endif