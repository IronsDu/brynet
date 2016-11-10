#include <assert.h>
#include <set>
#include <vector>
#include <map>
#include <thread>
#include <string.h>
#include <iostream>

#include "msgqueue.h"
#include "SocketLibFunction.h"
#include "fdset.h"
#include "systemlib.h"

using namespace std;

#include "connector.h"

class  ConnectorWorkThread final : public NonCopyable
{
public:
    typedef std::shared_ptr<ConnectorWorkThread>    PTR;

    ConnectorWorkThread(ThreadConnector::COMPLETED_CALLBACK);
    ~ConnectorWorkThread();

    void                checkConnectStatus(int timeout);
    bool                isConnectSuccess(sock clientfd);
    void                checkTimeout();
    void                pollConnectRequest(MsgQueue<AsyncConnectAddr>& connectRequests);

private:
    ThreadConnector::COMPLETED_CALLBACK    mCallback;

    struct ConnectingInfo
    {
        int64_t startConnectTime;
        int     timeout;
        int64_t uid;
    };

    std::map<sock, ConnectingInfo>  mConnectingInfos;
    std::set<sock>                  mConnectingFds;
    struct fdset_s*                 mFDSet;
};

ThreadConnector::ThreadConnector()
{
    mIsRun = false;
    mThread = nullptr;
}

ThreadConnector::~ThreadConnector()
{
    destroy();
}

ConnectorWorkThread::ConnectorWorkThread(ThreadConnector::COMPLETED_CALLBACK callback) : mCallback(callback)
{
    mFDSet = ox_fdset_new();
}

ConnectorWorkThread::~ConnectorWorkThread()
{
    ox_fdset_delete(mFDSet);
    mFDSet = nullptr;
}

bool ConnectorWorkThread::isConnectSuccess(sock clientfd)
{
    bool connect_ret = false;

    if (ox_fdset_check(mFDSet, clientfd, WriteCheck))
    {
        int error;
        int len = sizeof(error);
        if (getsockopt(clientfd, SOL_SOCKET, SO_ERROR, (char*)&error, (socklen_t*)&len) != -1)
        {
            connect_ret = error == 0;
        }
    }

    return connect_ret;
}

void ConnectorWorkThread::checkConnectStatus(int timeout)
{
    if (ox_fdset_poll(mFDSet, timeout) <= 0)
    {
        return;
    }

    set<sock>       complete_fds;   /*  完成队列    */
    set<sock>       failed_fds;     /*  失败队列    */

    for (auto& v : mConnectingFds)
    {
        if (ox_fdset_check(mFDSet, v, ErrorCheck))
        {
            complete_fds.insert(v);
            failed_fds.insert(v);
        } 
        else if (ox_fdset_check(mFDSet, v, WriteCheck))
        {
            complete_fds.insert(v);
            if (!isConnectSuccess(v))
            {
                failed_fds.insert(v);
            }
        }
    }

    for (auto fd : complete_fds)
    {
        ox_fdset_del(mFDSet, fd, WriteCheck | ErrorCheck);

        map<sock, ConnectingInfo>::iterator it = mConnectingInfos.find(fd);
        if (it != mConnectingInfos.end())
        {
            if (failed_fds.find(fd) != failed_fds.end())
            {
                ox_socket_close(fd);
                mCallback(-1, it->second.uid);
            }
            else
            {
                mCallback(fd, it->second.uid);
            }

            mConnectingInfos.erase(it);
        }

        mConnectingFds.erase(fd);
    }
}

void ConnectorWorkThread::checkTimeout()
{
    int64_t now_time = ox_getnowtime();

    for (map<sock, ConnectingInfo>::iterator it = mConnectingInfos.begin(); it != mConnectingInfos.end();)
    {
        if ((now_time - it->second.startConnectTime) >= it->second.timeout)
        {
            sock fd = it->first;
            int64_t uid = it->second.uid;

            ox_fdset_del(mFDSet, fd, WriteCheck | ErrorCheck);

            mConnectingFds.erase(fd);
            mConnectingInfos.erase(it++);

            ox_socket_close(fd);

            mCallback(SOCKET_ERROR, uid);
        }
        else
        {
            ++it;
        }
    }
}

void ConnectorWorkThread::pollConnectRequest(MsgQueue<AsyncConnectAddr>& connectRequests)
{
    AsyncConnectAddr addr;
    while (connectRequests.PopBack(addr))
    {
        bool addToFDSet = false;
        bool connectSuccess = false;

        struct sockaddr_in server_addr;
        sock clientfd = SOCKET_ERROR;

        ox_socket_init();

        clientfd = ox_socket_create(AF_INET, SOCK_STREAM, 0);
        ox_socket_nonblock(clientfd);

        if (clientfd != SOCKET_ERROR)
        {
            server_addr.sin_family = AF_INET;
            server_addr.sin_addr.s_addr = inet_addr(addr.getIP().c_str());
            server_addr.sin_port = htons(addr.getPort());

            int n = connect(clientfd, (struct sockaddr*)&server_addr, sizeof(struct sockaddr));
            if (n < 0)
            {
                int check_error = 0;
#if defined PLATFORM_WINDOWS
                check_error = WSAEWOULDBLOCK;
#else
                check_error = EINPROGRESS;
#endif
                if (check_error != sErrno)
                {
                    ox_socket_close(clientfd);
                    clientfd = SOCKET_ERROR;
                }
                else
                {
                    ConnectingInfo ci;
                    ci.startConnectTime = ox_getnowtime();
                    ci.uid = addr.getUID();
                    ci.timeout = addr.getTimeout();

                    mConnectingInfos[clientfd] = ci;
                    mConnectingFds.insert(clientfd);

                    ox_fdset_add(mFDSet, clientfd, WriteCheck | ErrorCheck);
                    addToFDSet = true;
                }
            }
            else if (n == 0)
            {
                connectSuccess = true;
            }
        }

        if (connectSuccess)
        {
            mCallback(clientfd, addr.getUID());
        }
        else
        {
            if (!addToFDSet)
            {
                mCallback(SOCKET_ERROR, addr.getUID());
            }
        }
    }
}

void ThreadConnector::run(ConnectorWorkThread::PTR cwt)
{
    while (mIsRun)
    {
        mThreadEventloop.loop(10);

        cwt->checkConnectStatus(0);
        mConnectRequests.SyncRead(0);
        cwt->pollConnectRequest(mConnectRequests);

        cwt->checkTimeout();
    }
}

void ThreadConnector::startThread(COMPLETED_CALLBACK callback)
{
    if (mThread == nullptr)
    {
        mIsRun = true;

        mThread = new std::thread([](ThreadConnector::PTR tc, ConnectorWorkThread::PTR cwt){
            tc->run(cwt);
        }, shared_from_this(), std::make_shared<ConnectorWorkThread>(callback));
    }
}

void ThreadConnector::destroy()
{
    if (mThread != nullptr)
    {
        mIsRun = false;
        if (mThread->joinable())
        {
            mThread->join();
        }
        delete mThread;
    }
}

void ThreadConnector::asyncConnect(const char* ip, int port, int ms, int64_t uid)
{
    mConnectRequests.Push(AsyncConnectAddr(ip, port, ms, uid));
    mConnectRequests.ForceSyncWrite();
    mThreadEventloop.wakeup();
}