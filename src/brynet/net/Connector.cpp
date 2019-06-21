#include <cassert>
#include <set>
#include <map>
#include <thread>

#include <brynet/net/SocketLibFunction.h>
#include <brynet/net/poller.h>
#include <brynet/net/Exception.h>

#include <brynet/net/Connector.h>

namespace brynet { namespace net {

    class AsyncConnectAddr final
    {
    public:
        AsyncConnectAddr(std::string&& ip,
            int port,
            std::chrono::nanoseconds timeout,
            AsyncConnector::CompletedCallback&& successCB,
            AsyncConnector::FailedCallback&& failedCB,
            std::vector<AsyncConnector::ProcessTcpSocketCallback>&& processCallbacks)
            :
            mIP(std::forward<std::string>(ip)),
            mPort(port),
            mTimeout(timeout),
            mSuccessCB(std::forward<AsyncConnector::CompletedCallback>(successCB)),
            mFailedCB(std::forward<AsyncConnector::FailedCallback>(failedCB)),
            mProcessCallbacks(std::forward<std::vector<AsyncConnector::ProcessTcpSocketCallback>>(processCallbacks))
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

        const std::vector<AsyncConnector::ProcessTcpSocketCallback>& getProcessCallbacks() const
        {
            return mProcessCallbacks;
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
        const std::vector<AsyncConnector::ProcessTcpSocketCallback>    mProcessCallbacks;
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

        class ConnectingInfo
        {
        public:
            ConnectingInfo()
            {
                timeout = std::chrono::nanoseconds::zero();
            }

            std::chrono::steady_clock::time_point   startConnectTime;
            std::chrono::nanoseconds                timeout;
            AsyncConnector::CompletedCallback       successCB;
            AsyncConnector::FailedCallback          failedCB;
            std::vector<AsyncConnector::ProcessTcpSocketCallback> processCallbacks;
        };

        std::map<sock, ConnectingInfo>              mConnectingInfos;

        class PollerDeleter
        {
        public:
            void operator()(struct poller_s* ptr) const
            {
                ox_poller_delete(ptr);
            }
        };
        class StackDeleter
        {
        public:
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
            else
            {
                auto socket = TcpSocket::Create(fd, false);
                const auto& connectingInfo = it->second;
                if (successFds.find(fd) != successFds.end())
                {
                    for (const auto& process : connectingInfo.processCallbacks)
                    {
                        process(*socket);
                    }
                    if (connectingInfo.successCB != nullptr)
                    {
                        connectingInfo.successCB(std::move(socket));
                    }
                }
                else
                {
                    if (connectingInfo.failedCB != nullptr)
                    {
                        connectingInfo.failedCB();
                    }
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

#ifdef PLATFORM_WINDOWS
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
            ci.processCallbacks = addr.getProcessCallbacks();

            mConnectingInfos[clientfd] = ci;
            ox_poller_add(mPoller.get(), clientfd, WriteCheck);

            return;
        }

        if (addr.getSuccessCB() != nullptr)
        {
            auto tcpSocket = TcpSocket::Create(clientfd, false);
            for (const auto& process : addr.getProcessCallbacks())
            {
                process(*tcpSocket);
            }
            addr.getSuccessCB()(std::move(tcpSocket));
        }
        return;

    FAILED:
        if (clientfd != INVALID_SOCKET)
        {
            brynet::net::base::SocketClose(clientfd);
            clientfd = INVALID_SOCKET;
            (void)clientfd;
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
        catch (std::system_error& e)
        {
            (void)e;
        }

        mEventLoop = nullptr;
        mWorkInfo = nullptr;
        mIsRun = nullptr;
        mThread = nullptr;
    }

    class AsyncConnector::ConnectOptions::Options
    {
    public:
        Options()
        {
            timeout = std::chrono::seconds(10);
        }

        std::string ip;
        int port;
        std::chrono::nanoseconds timeout;
        std::vector<ProcessTcpSocketCallback> processCallbacks;
        CompletedCallback completedCallback;
        FailedCallback faledCallback;
    };

    AsyncConnector::ConnectOptions::ConnectOptionFunc AsyncConnector::ConnectOptions::WithAddr(const std::string& ip, int port)
    {
        return [ip, port](AsyncConnector::ConnectOptions::Options& option) {
            option.ip = ip;
            option.port = port;
        };
    }

    AsyncConnector::ConnectOptions::ConnectOptionFunc AsyncConnector::ConnectOptions::WithTimeout(std::chrono::nanoseconds timeout)
    {
        return [timeout](AsyncConnector::ConnectOptions::Options& option) {
            option.timeout = timeout;
        };
    }

    AsyncConnector::ConnectOptions::ConnectOptionFunc 
        AsyncConnector::ConnectOptions::WithCompletedCallback(CompletedCallback&& callback)
    {
        return [callback](AsyncConnector::ConnectOptions::Options& option) {
            option.completedCallback = callback;
        };
    }

    AsyncConnector::ConnectOptions::ConnectOptionFunc 
        AsyncConnector::ConnectOptions::AddProcessTcpSocketCallback(ProcessTcpSocketCallback&& process)
    {
        return [process](AsyncConnector::ConnectOptions::Options& option) {
            option.processCallbacks.push_back(process);
        };
    }

    AsyncConnector::ConnectOptions::ConnectOptionFunc 
        AsyncConnector::ConnectOptions::WithFailedCallback(FailedCallback&& callback)
    {
        return [callback](AsyncConnector::ConnectOptions::Options& option) {
            option.faledCallback = callback;
        };
    }

    std::chrono::nanoseconds AsyncConnector::ConnectOptions::ExtractTimeout(const std::vector<ConnectOptions::ConnectOptionFunc>& options)
    {
        Options option;
        for (const auto& func : options)
        {
            func(option);
        }
        return option.timeout;
    }

    void AsyncConnector::asyncConnect(const std::vector<ConnectOptions::ConnectOptionFunc>& options)
    {
#ifdef HAVE_LANG_CXX17
        std::shared_lock<std::shared_mutex> lck(mThreadGuard);
#else
        std::lock_guard<std::mutex> lck(mThreadGuard);
#endif
        AsyncConnector::ConnectOptions::Options option;
        for (const auto& func : options)
        {
            func(option);
        }

        if (option.completedCallback == nullptr && option.faledCallback == nullptr)
        {
            throw ConnectException("all callback is nullptr");
        }
        if (option.ip.empty())
        {
            throw ConnectException("addr is empty");
        }

        if (!(*mIsRun))
        {
            throw ConnectException("work thread already stop");
        }

        auto workInfo = mWorkInfo;
        auto address = AsyncConnectAddr(std::move(option.ip),
            option.port,
            option.timeout,
            std::move(option.completedCallback),
            std::move(option.faledCallback),
            std::move(option.processCallbacks));
        mEventLoop->runAsyncFunctor([workInfo, address]() {
            workInfo->processConnect(address);
        });
    }

    AsyncConnector::Ptr AsyncConnector::Create()
    {
        class make_shared_enabler : public AsyncConnector {};
        return std::make_shared<make_shared_enabler>();
    }

} }