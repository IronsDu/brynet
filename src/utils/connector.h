#ifndef _CONNECTOR_H
#define _CONNECTOR_H

#include <set>
#include <map>
#include <functional>
#include <string>
#include <memory>

#include "EventLoop.h"
#include "msgqueue.h"
#include "NonCopyable.h"
#include "SocketLibTypes.h"

class AsyncConnectAddr
{
public:
    AsyncConnectAddr()
    {
        mPort = 0;
        mTimeout = 0;
        mUid = -1;
    }

    AsyncConnectAddr(const char* ip, int port, int timeout, int64_t uid) : mIP(ip), mPort(port), mTimeout(timeout), mUid(uid)
    {
    }

    const std::string&   getIP() const
    {
        return mIP;
    }

    int             getPort() const
    {
        return mPort;
    }

    int64_t         getUID() const
    {
        return mUid;
    }

    int             getTimeout() const
    {
        return mTimeout;
    }

private:
    std::string mIP;
    int         mPort;
    int         mTimeout;
    int64_t     mUid;
};

class ThreadConnector : NonCopyable
{
public:
    typedef std::shared_ptr<ThreadConnector> PTR;

    ThreadConnector(std::function<void(sock, int64_t)> callback);

    virtual ~ThreadConnector();
    void                startThread();
    void                destroy();

    void                asyncConnect(const char* ip, int port, int ms, int64_t uid);

private:
    void                checkConnectStatus(struct fdset_s* fdset, int timeout);
    bool                isConnectSuccess(struct fdset_s* fdset, sock clientfd);
    void                pollConnectRequest();
    void                checkTimeout();

    void                run();
    static  void        s_thread(void* arg);

private:

    struct ConnectingInfo 
    {
        int64_t startConnectTime;
        int     timeout;
        int64_t uid;
    };

    MsgQueue<AsyncConnectAddr>      mConnectRequests;     /*  «Î«Û¡–±Ì    */

    std::map<sock, ConnectingInfo>  mConnectingInfos;
    std::set<sock>                  mConnectingFds;
    std::function<void(sock, int64_t)>    mCallback;
    EventLoop                       mThreadEventloop;
    struct fdset_s*                 mFDSet;

    std::thread*                    mThread;
    bool                            mIsRun;
};

#endif