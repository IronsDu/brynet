#include <cassert>
#include <set>
#include <vector>
#include <map>
#include <thread>
#include <string>
#include <cstring>

#include <brynet/net/SocketLibFunction.h>
#include <brynet/net/fdset.h>

#include <brynet/net/Connector.h>

using namespace brynet;
using namespace brynet::net;

namespace brynet
{
    namespace net
    {
        class AsyncConnectAddr
        {
        public:
            AsyncConnectAddr() BRYNET_NOEXCEPT : mTimeout(std::chrono::nanoseconds::zero())
            {
                mPort = 0;
            }

            AsyncConnectAddr(const std::string& ip, 
                int port, 
                std::chrono::nanoseconds timeout, 
                const AsyncConnector::COMPLETED_CALLBACK& successCB,
                const AsyncConnector::FAILED_CALLBACK& failedCB) : 
                mIP(ip),
                mPort(port), 
                mTimeout(timeout), 
                mSuccessCB(successCB),
                mFailedCB(failedCB)
            {
            }

            const std::string&         getIP() const
            {
                return mIP;
            }

            int                         getPort() const
            {
                return mPort;
            }

            const AsyncConnector::COMPLETED_CALLBACK&   getSuccessCB() const
            {
                return mSuccessCB;
            }

            const AsyncConnector::FAILED_CALLBACK&  getFailedCB() const
            {
                return mFailedCB;
            }

            std::chrono::nanoseconds                getTimeout() const
            {
                return mTimeout;
            }

        private:
            std::string                         mIP;
            int                                 mPort;
            std::chrono::nanoseconds            mTimeout;
            AsyncConnector::COMPLETED_CALLBACK  mSuccessCB;
            AsyncConnector::FAILED_CALLBACK     mFailedCB;
        };

        class ConnectorWorkInfo final : public NonCopyable
        {
        public:
            typedef std::shared_ptr<ConnectorWorkInfo>    PTR;

            ConnectorWorkInfo() BRYNET_NOEXCEPT;

            void                checkConnectStatus(int millsecond);
            bool                isConnectSuccess(sock clientfd) const;
            void                checkTimeout();
            void                processConnect(const AsyncConnectAddr&);
            void                causeAllFailed();

        private:

            struct ConnectingInfo
            {
                ConnectingInfo()
                {
                    timeout = std::chrono::nanoseconds::zero();
                }

                std::chrono::steady_clock::time_point   startConnectTime;
                std::chrono::nanoseconds                timeout;
                AsyncConnector::COMPLETED_CALLBACK      successCB;
                AsyncConnector::FAILED_CALLBACK         failedCB;
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

ConnectorWorkInfo::ConnectorWorkInfo() BRYNET_NOEXCEPT
{
    mFDSet.reset(ox_fdset_new());
}

bool ConnectorWorkInfo::isConnectSuccess(sock clientfd) const
{
    if (!ox_fdset_check(mFDSet.get(), clientfd, WriteCheck))
    {
        return false;
    }

    int error;
    int len = sizeof(error);
    if (getsockopt(clientfd, SOL_SOCKET, SO_ERROR, (char*)&error, (socklen_t*)&len) == -1)
    {
        return false;
    }

    return error == 0;
}

void ConnectorWorkInfo::checkConnectStatus(int millsecond)
{
    if (ox_fdset_poll(mFDSet.get(), millsecond) <= 0)
    {
        return;
    }

    std::set<sock>  total_fds;
    std::set<sock>  success_fds;

    for (auto& v : mConnectingFds)
    {
        if (ox_fdset_check(mFDSet.get(), v, ErrorCheck))
        {
            total_fds.insert(v);
        } 
        else if (ox_fdset_check(mFDSet.get(), v, WriteCheck))
        {
            total_fds.insert(v);
            if (isConnectSuccess(v))
            {
                success_fds.insert(v);
            }
        }
    }

    for (auto fd : total_fds)
    {
        ox_fdset_del(mFDSet.get(), fd, WriteCheck | ErrorCheck);
        mConnectingFds.erase(fd);

        auto it = mConnectingInfos.find(fd);
        if (it == mConnectingInfos.end())
        {
            continue;
        }

        if (success_fds.find(fd) != success_fds.end())
        {
            if (it->second.successCB != nullptr)
            {
                it->second.successCB(fd);
            }
        }
        else
        {
            ox_socket_close(fd);
            if (it->second.failedCB != nullptr)
            {
                it->second.failedCB();
            }
        }
        
        mConnectingInfos.erase(it);
    }
}

void ConnectorWorkInfo::checkTimeout()
{
    for (auto it = mConnectingInfos.begin(); it != mConnectingInfos.end();)
    {
        auto now = std::chrono::steady_clock::now();
        if ((now - it->second.startConnectTime) < it->second.timeout)
        {
            ++it;
            continue;
        }

        auto fd = it->first;
        auto cb = it->second.failedCB;

        ox_fdset_del(mFDSet.get(), fd, WriteCheck | ErrorCheck);

        mConnectingFds.erase(fd);
        mConnectingInfos.erase(it++);

        ox_socket_close(fd);
        if (cb != nullptr)
        {
            cb();
        }
    }
}

void ConnectorWorkInfo::causeAllFailed()
{
    for (auto it = mConnectingInfos.begin(); it != mConnectingInfos.end();)
    {
        auto fd = it->first;
        auto cb = it->second.failedCB;

        ox_fdset_del(mFDSet.get(), fd, WriteCheck | ErrorCheck);

        mConnectingFds.erase(fd);
        mConnectingInfos.erase(it++);

        ox_socket_close(fd);
        if (cb != nullptr)
        {
            cb();
        }
    }
}

void ConnectorWorkInfo::processConnect(const AsyncConnectAddr& addr)
{
    struct sockaddr_in server_addr;
    sock clientfd = SOCKET_ERROR;
    ConnectingInfo ci;

#if defined PLATFORM_WINDOWS
    int check_error = WSAEWOULDBLOCK;
#else
    int check_error = EINPROGRESS;
#endif
    int n = 0;

    ox_socket_init();

    clientfd = ox_socket_create(AF_INET, SOCK_STREAM, 0);
    if (clientfd == SOCKET_ERROR)
    {
        goto FAILED;
    }

    ox_socket_nonblock(clientfd);
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(addr.getIP().c_str());
    server_addr.sin_port = htons(addr.getPort());

    n = connect(clientfd, (struct sockaddr*)&server_addr, sizeof(struct sockaddr));
    if (n == 0)
    {
        goto SUCCESS;
    }

    if (check_error != sErrno)
    {
        ox_socket_close(clientfd);
        clientfd = SOCKET_ERROR;
        goto FAILED;
    }

    ci.startConnectTime = std::chrono::steady_clock::now();
    ci.successCB = addr.getSuccessCB();
    ci.failedCB = addr.getFailedCB();
    ci.timeout = addr.getTimeout();

    mConnectingInfos[clientfd] = ci;
    mConnectingFds.insert(clientfd);
    ox_fdset_add(mFDSet.get(), clientfd, WriteCheck | ErrorCheck);
    return;

SUCCESS:
    if (addr.getSuccessCB() != nullptr)
    {
        addr.getSuccessCB()(clientfd);
    }
    return;

FAILED:
    if (addr.getFailedCB() != nullptr)
    {
        addr.getFailedCB()();
    }
}

AsyncConnector::AsyncConnector()
{
    mIsRun = false;
}

AsyncConnector::~AsyncConnector()
{
    stopWorkerThread();
}

void AsyncConnector::run()
{
    while (mIsRun)
    {
        mEventLoop->loop(10);
        mWorkInfo->checkConnectStatus(0);
        mWorkInfo->checkTimeout();
    }
    mWorkInfo->causeAllFailed();
}

void AsyncConnector::startWorkerThread()
{
#ifdef HAVE_LANG_CXX17
    std::lock_guard<std::shared_mutex> lck(mThreadGuard);
#else
    std::lock_guard<std::mutex> lck(mThreadGuard);
#endif

    if (mThread != nullptr)
    {
        return;
    }

    mIsRun = true;
    mWorkInfo = std::make_shared<ConnectorWorkInfo>();
    mEventLoop = std::make_shared<EventLoop>();
    auto shared_this = shared_from_this();
    mThread = std::make_shared<std::thread>([shared_this](){
        shared_this->run();
    });
}

void AsyncConnector::stopWorkerThread()
{
#ifdef HAVE_LANG_CXX17
    std::lock_guard<std::shared_mutex> lck(mThreadGuard);
#else
    std::lock_guard<std::mutex> lck(mThreadGuard);
#endif

    if (mThread == nullptr)
    {
        return;
    }

    mEventLoop->pushAsyncProc([this]() {
        mIsRun = false;
    });

    if (mThread->joinable())
    {
        mThread->join();
    }
    mEventLoop = nullptr;
    mThread = nullptr;
    mWorkInfo = nullptr;
}

void AsyncConnector::asyncConnect(const std::string& ip, 
    int port, 
    std::chrono::nanoseconds timeout,
    COMPLETED_CALLBACK successCB, 
    FAILED_CALLBACK failedCB)
{
#ifdef HAVE_LANG_CXX17
    std::shared_lock<std::shared_mutex> lck(mThreadGuard);
#else
    std::lock_guard<std::mutex> lck(mThreadGuard);
#endif

    if (successCB == nullptr || failedCB == nullptr)
    {
        throw std::runtime_error("all callback is nullptr");
    }

    if (!mIsRun)
    {
        throw std::runtime_error("work thread already stop");
    }

    auto shared_this = shared_from_this();
    auto address = AsyncConnectAddr(ip,
        port,
        timeout,
        successCB,
        failedCB);
    mEventLoop->pushAsyncProc([shared_this, 
        address]() {
        shared_this->mWorkInfo->processConnect(address);
    });
}

AsyncConnector::PTR AsyncConnector::Create()
{
    struct make_shared_enabler : public AsyncConnector {};
    return std::make_shared<make_shared_enabler>();
}