#include <cassert>
#include <set>
#include <map>
#include <thread>

#include <brynet/net/SocketLibFunction.h>
#include <brynet/net/poller.h>

#include <brynet/net/Connector.h>

namespace brynet { namespace net {

    class AsyncConnectAddr final
    {
    public:
        AsyncConnectAddr(std::string ip,
            int port,
            std::chrono::nanoseconds timeout,
            AsyncConnector::CompletedCallback successCB,
            AsyncConnector::FailedCallback failedCB)
            :
            mIP(std::move(ip)),
            mPort(port),
            mTimeout(timeout),
            mSuccessCB(std::move(successCB)),
            mFailedCB(std::move(failedCB))
        {
        }

        const std::string&                          getIP() const
        {
            return mIP;
        }

        int                                         getPort() const
        {
            return mPort;
        }

        const AsyncConnector::CompletedCallback&    getSuccessCB() const
        {
            return mSuccessCB;
        }

        const AsyncConnector::FailedCallback&       getFailedCB() const
        {
            return mFailedCB;
        }

        std::chrono::nanoseconds                    getTimeout() const
        {
            return mTimeout;
        }

    private:
        const std::string                           mIP;
        const int                                   mPort;
        const std::chrono::nanoseconds              mTimeout;
        const AsyncConnector::CompletedCallback     mSuccessCB;
        const AsyncConnector::FailedCallback        mFailedCB;
    };

    class ConnectorWorkInfo final : public utils::NonCopyable
    {
    public:
        using Ptr = std::shared_ptr<ConnectorWorkInfo>;

        ConnectorWorkInfo() BRYNET_NOEXCEPT;

        void                checkConnectStatus(int millsecond);
        bool                isConnectSuccess(sock clientfd, bool willCheckWrite) const;
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
            AsyncConnector::CompletedCallback       successCB;
            AsyncConnector::FailedCallback          failedCB;
        };

        std::map<sock, ConnectingInfo>              mConnectingInfos;

        struct PollerDeleter
        {
            void operator()(struct poller_s* ptr) const
            {
                ox_poller_delete(ptr);
            }
        };
        struct StackDeleter
        {
            void operator()(struct stack_s* ptr) const
            {
                ox_stack_delete(ptr);
            }
        };

        std::unique_ptr<struct poller_s, PollerDeleter> mPoller;
        std::unique_ptr<struct stack_s, StackDeleter> mPollResult;
    };

    ConnectorWorkInfo::ConnectorWorkInfo() BRYNET_NOEXCEPT
    {
        mPoller.reset(ox_poller_new());
        mPollResult.reset(ox_stack_new(1024, sizeof(sock)));
    }

    bool ConnectorWorkInfo::isConnectSuccess(sock clientfd, bool willCheckWrite) const
    {
        if (willCheckWrite && !ox_poller_check(mPoller.get(), clientfd, WriteCheck))
        {
            return false;
        }

        int error = SOCKET_ERROR;
        int len = sizeof(error);
        if (getsockopt(clientfd, SOL_SOCKET, SO_ERROR, (char*)&error, (socklen_t*)&len) == SOCKET_ERROR)
        {
            return false;
        }

        return error == 0;
    }

    void ConnectorWorkInfo::checkConnectStatus(int millsecond)
    {
        if (ox_poller_poll(mPoller.get(), millsecond) <= 0)
        {
            return;
        }

        std::set<sock>  totalFds;
        std::set<sock>  successFds;

        ox_poller_visitor(mPoller.get(), WriteCheck, mPollResult.get());
        while (true)
        {
            auto p = ox_stack_popfront(mPollResult.get());
            if (p == nullptr)
            {
                break;
            }

            const sock fd = *(sock*)p;
            totalFds.insert(fd);
            if (isConnectSuccess(fd, false) && !brynet::net::base::IsSelfConnect(fd))
            {
                successFds.insert(fd);
            }
        }

        for (auto fd : totalFds)
        {
            ox_poller_remove(mPoller.get(), fd);

            auto it = mConnectingInfos.find(fd);
            if (it == mConnectingInfos.end())
            {
                continue;
            }

            auto socket = TcpSocket::Create(fd, false);
            if (successFds.find(fd) != successFds.end())
            {
                if (it->second.successCB != nullptr)
                {
                    it->second.successCB(std::move(socket));
                }
            }
            else
            {
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
            const auto now = std::chrono::steady_clock::now();
            if ((now - it->second.startConnectTime) < it->second.timeout)
            {
                ++it;
                continue;
            }

            auto fd = it->first;
            auto cb = it->second.failedCB;

            ox_poller_remove(mPoller.get(), fd);
            mConnectingInfos.erase(it++);

            brynet::net::base::SocketClose(fd);
            if (cb != nullptr)
            {
                //TODO::don't modify mConnectingInfos in cb
                cb();
            }
        }
    }

    void ConnectorWorkInfo::causeAllFailed()
    {
        auto copyMap = mConnectingInfos;
        mConnectingInfos.clear();

        for (const auto& v : copyMap)
        {
            auto fd = v.first;
            auto cb = v.second.failedCB;

            ox_poller_remove(mPoller.get(), fd);
            brynet::net::base::SocketClose(fd);
            if (cb != nullptr)
            {
                cb();
            }
        }
    }

    void ConnectorWorkInfo::processConnect(const AsyncConnectAddr& addr)
    {
        struct sockaddr_in server_addr = { 0 };
        sock clientfd = INVALID_SOCKET;

#if defined PLATFORM_WINDOWS
        const int ExpectedError = WSAEWOULDBLOCK;
#else
        const int ExpectedError = EINPROGRESS;
#endif
        int n = 0;

        brynet::net::base::InitSocket();

        clientfd = brynet::net::base::SocketCreate(AF_INET, SOCK_STREAM, 0);
        if (clientfd == INVALID_SOCKET)
        {
            goto FAILED;
        }

        brynet::net::base::SocketNonblock(clientfd);
        server_addr.sin_family = AF_INET;
        inet_pton(AF_INET, addr.getIP().c_str(), &server_addr.sin_addr.s_addr);
        server_addr.sin_port = htons(addr.getPort());

        n = connect(clientfd, (struct sockaddr*)&server_addr, sizeof(struct sockaddr));
        if (n == 0)
        {
            if (brynet::net::base::IsSelfConnect(clientfd))
            {
                goto FAILED;
            }
            else
            {
                goto SUCCESS;
            }
        }
        else if (sErrno != ExpectedError)
        {
            goto FAILED;
        }
        else
        {
            ConnectingInfo ci;
            ci.startConnectTime = std::chrono::steady_clock::now();
            ci.successCB = addr.getSuccessCB();
            ci.failedCB = addr.getFailedCB();
            ci.timeout = addr.getTimeout();

            mConnectingInfos[clientfd] = ci;
            ox_poller_add(mPoller.get(), clientfd, WriteCheck);

            return;
        }

    SUCCESS:
        if (addr.getSuccessCB() != nullptr)
        {
            addr.getSuccessCB()(TcpSocket::Create(clientfd, false));
        }
        return;

    FAILED:
        if (clientfd != INVALID_SOCKET)
        {
            brynet::net::base::SocketClose(clientfd);
            clientfd = INVALID_SOCKET;
        }
        if (addr.getFailedCB() != nullptr)
        {
            addr.getFailedCB()();
        }
    }

    AsyncConnector::AsyncConnector()
    {
        mIsRun = std::make_shared<bool>(false);
    }

    AsyncConnector::~AsyncConnector()
    {
        stopWorkerThread();
    }

    static void runOnceCheckConnect(const std::shared_ptr<brynet::net::EventLoop>& eventLoop,
        const std::shared_ptr<ConnectorWorkInfo>& workerInfo)
    {
        eventLoop->loop(std::chrono::milliseconds(10).count());
        workerInfo->checkConnectStatus(0);
        workerInfo->checkTimeout();
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

        mIsRun = std::make_shared<bool>(true);
        mWorkInfo = std::make_shared<ConnectorWorkInfo>();
        mEventLoop = std::make_shared<EventLoop>();

        auto eventLoop = mEventLoop;
        auto workerInfo = mWorkInfo;
        auto isRun = mIsRun;

        mThread = std::make_shared<std::thread>([eventLoop, workerInfo, isRun]() {
            while (*isRun)
            {
                runOnceCheckConnect(eventLoop, workerInfo);
            }

            workerInfo->causeAllFailed();
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

        mEventLoop->runAsyncFunctor([this]() {
            *mIsRun = false;
        });

        try
        {
            if (mThread->joinable())
            {
                mThread->join();
            }
        }
        catch (...)
        {
        }

        mEventLoop = nullptr;
        mWorkInfo = nullptr;
        mIsRun = nullptr;
        mThread = nullptr;
    }

    void AsyncConnector::asyncConnect(const std::string& ip,
        int port,
        std::chrono::nanoseconds timeout,
        CompletedCallback successCB,
        FailedCallback failedCB)
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

        if (!(*mIsRun))
        {
            throw std::runtime_error("work thread already stop");
        }

        auto workInfo = mWorkInfo;
        auto address = AsyncConnectAddr(ip,
            port,
            timeout,
            successCB,
            failedCB);
        mEventLoop->runAsyncFunctor([workInfo, address]() {
            workInfo->processConnect(address);
        });
    }

    AsyncConnector::Ptr AsyncConnector::Create()
    {
        struct make_shared_enabler : public AsyncConnector {};
        return std::make_shared<make_shared_enabler>();
    }

} }