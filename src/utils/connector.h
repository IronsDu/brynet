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

    int                 getPort() const
    {
        return mPort;
    }

    int64_t             getUID() const
    {
        return mUid;
    }

    int                 getTimeout() const
    {
        return mTimeout;
    }

private:
    std::string         mIP;
    int                 mPort;
    int                 mTimeout;
    int64_t             mUid;
};

class ConnectorWorkThread;

class ThreadConnector : NonCopyable, public std::enable_shared_from_this<ThreadConnector>
{
public:
    typedef std::shared_ptr<ThreadConnector> PTR;
    typedef std::function<void(sock, int64_t)> COMPLETED_CALLBACK;

    ThreadConnector();

    virtual ~ThreadConnector();
    void                startThread(COMPLETED_CALLBACK callback);
    void                destroy();

    void                asyncConnect(const char* ip, int port, int ms, int64_t uid);

private:
    void                run(std::shared_ptr<ConnectorWorkThread>);

private:

    MsgQueue<AsyncConnectAddr>      mConnectRequests;     /*  «Î«Û¡–±Ì    */
    EventLoop                       mThreadEventloop;

    std::thread*                    mThread;
    bool                            mIsRun;
};

#endif