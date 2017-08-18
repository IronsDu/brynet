#include <cassert>
#include <set>
#include <vector>
#include <map>
#include <thread>
#include <string>
#include <cstring>

#include "MsgQueue.h"
#include "SocketLibFunction.h"
#include "fdset.h"
#include "systemlib.h"

#include "Connector.h"

using namespace brynet;
using namespace brynet::net;

namespace brynet
{
    namespace net
    {
        class AsyncConnectAddr
        {
        public:
            AsyncConnectAddr() noexcept
            {
                mPort = 0;
            }

            AsyncConnectAddr(const char* ip, int port, std::chrono::milliseconds timeout, std::any ud) : mIP(ip), mPort(port), mTimeout(timeout), mUD(std::move(ud))
            {
            }

            const auto&     getIP() const
            {
                return mIP;
            }

            auto                getPort() const
            {
                return mPort;
            }

            const auto&         getUD() const
            {
                return mUD;
            }

            auto                getTimeout() const
            {
                return mTimeout;
            }

        private:
            std::string         mIP;
            int                 mPort;
            std::chrono::milliseconds   mTimeout;
            std::any            mUD;
        };

        class ConnectorWorkThread final : public NonCopyable
        {
        public:
            typedef std::shared_ptr<ConnectorWorkThread>    PTR;

            ConnectorWorkThread(AsyncConnector::COMPLETED_CALLBACK, AsyncConnector::FAILED_CALLBACK) noexcept;

            void                checkConnectStatus(int timeout);
            bool                isConnectSuccess(sock clientfd) const;
            void                checkTimeout();
            void                pollConnectRequest(std::shared_ptr<MsgQueue<AsyncConnectAddr>>& connectRequests);

        private:
            AsyncConnector::COMPLETED_CALLBACK      mCompletedCallback;
            AsyncConnector::FAILED_CALLBACK         mFailedCallback;

            struct ConnectingInfo
            {
                std::chrono::steady_clock::time_point startConnectTime;
                std::chrono::milliseconds     timeout;
                std::any ud;
            };

            std::map<sock, ConnectingInfo>  mConnectingInfos;
            std::set<sock>                  mConnectingFds;

            struct FDSetDeleter
            {
                void operator()(struct fdset_s* ptr) const
                {
                    ox_fdset_delete(ptr);
                }
            };

            std::unique_ptr<struct fdset_s, FDSetDeleter> mFDSet;
        };
    }
}

ConnectorWorkThread::ConnectorWorkThread(AsyncConnector::COMPLETED_CALLBACK completedCallback,
    AsyncConnector::FAILED_CALLBACK failedCallback) noexcept : 
    mCompletedCallback(std::move(completedCallback)), mFailedCallback(std::move(failedCallback))
{
    mFDSet.reset(ox_fdset_new());
}

bool ConnectorWorkThread::isConnectSuccess(sock clientfd) const
{
    bool connect_ret = false;

    if (ox_fdset_check(mFDSet.get(), clientfd, WriteCheck))
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
    if (ox_fdset_poll(mFDSet.get(), timeout) <= 0)
    {
        return;
    }

    std::set<sock>       complete_fds;   /*  完成队列    */
    std::set<sock>       failed_fds;     /*  失败队列    */

    for (auto& v : mConnectingFds)
    {
        if (ox_fdset_check(mFDSet.get(), v, ErrorCheck))
        {
            complete_fds.insert(v);
            failed_fds.insert(v);
        } 
        else if (ox_fdset_check(mFDSet.get(), v, WriteCheck))
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
        ox_fdset_del(mFDSet.get(), fd, WriteCheck | ErrorCheck);

        auto it = mConnectingInfos.find(fd);
        if (it != mConnectingInfos.end())
        {
            if (failed_fds.find(fd) != failed_fds.end())
            {
                ox_socket_close(fd);
                if (mFailedCallback != nullptr)
                {
                    mFailedCallback(it->second.ud);
                }
            }
            else
            {
                mCompletedCallback(fd, it->second.ud);
            }

            mConnectingInfos.erase(it);
        }

        mConnectingFds.erase(fd);
    }
}

void ConnectorWorkThread::checkTimeout()
{
    for (auto it = mConnectingInfos.begin(); it != mConnectingInfos.end();)
    {
        auto now = std::chrono::steady_clock::now();
        if ((now - it->second.startConnectTime) >= it->second.timeout)
        {
            auto fd = it->first;
            auto uid = it->second.ud;

            ox_fdset_del(mFDSet.get(), fd, WriteCheck | ErrorCheck);

            mConnectingFds.erase(fd);
            mConnectingInfos.erase(it++);

            ox_socket_close(fd);
            if (mFailedCallback != nullptr)
            {
                mFailedCallback(uid);
            }
        }
        else
        {
            ++it;
        }
    }
}

void ConnectorWorkThread::pollConnectRequest(std::shared_ptr<MsgQueue<AsyncConnectAddr>>& connectRequests)
{
    AsyncConnectAddr addr;
    while (connectRequests->popBack(addr))
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
                    ci.startConnectTime = std::chrono::steady_clock::now();
                    ci.ud = addr.getUD();
                    ci.timeout = addr.getTimeout();

                    mConnectingInfos[clientfd] = ci;
                    mConnectingFds.insert(clientfd);

                    ox_fdset_add(mFDSet.get(), clientfd, WriteCheck | ErrorCheck);
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
            mCompletedCallback(clientfd, addr.getUD());
        }
        else
        {
            if (!addToFDSet && mFailedCallback != nullptr)
            {
                mFailedCallback(addr.getUD());
            }
        }
    }
}

AsyncConnector::AsyncConnector()
{
    mIsRun = false;
    mConnectRequests = std::make_shared<MsgQueue<AsyncConnectAddr>>();
}

AsyncConnector::~AsyncConnector()
{
    destroy();
}

void AsyncConnector::run(std::shared_ptr<ConnectorWorkThread> cwt)
{
    while (mIsRun)
    {
        mEventLoop.loop(10);

        cwt->checkConnectStatus(0);
        mConnectRequests->syncRead(0);
        cwt->pollConnectRequest(mConnectRequests);

        cwt->checkTimeout();
    }
}

void AsyncConnector::startThread(COMPLETED_CALLBACK completedCallback, FAILED_CALLBACK failedCallback)
{
    std::lock_guard<std::mutex> lck(mThreadGuard);
    if (mThread == nullptr)
    {
        mIsRun = true;

        mThread = std::make_shared<std::thread>([shared_this = shared_from_this(),
            cwt = std::make_shared<ConnectorWorkThread>(std::move(completedCallback), std::move(failedCallback))](){
            shared_this->run(cwt);
        });
    }
}

void AsyncConnector::destroy()
{
    std::lock_guard<std::mutex> lck(mThreadGuard);
    if (mThread != nullptr)
    {
        mIsRun = false;
        if (mThread->joinable())
        {
            mThread->join();
        }
        mThread = nullptr;
    }
}

void AsyncConnector::asyncConnect(const char* ip, int port, int ms, std::any ud)
{
    mConnectRequests->push(AsyncConnectAddr(ip, port, std::chrono::milliseconds(ms), ud));
    mConnectRequests->forceSyncWrite();
    mEventLoop.wakeup();
}

AsyncConnector::PTR AsyncConnector::Create()
{
    struct make_shared_enabler : public AsyncConnector {};
    return std::make_shared<make_shared_enabler>();
}