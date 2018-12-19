#include <cassert>
#include <set>
#include <map>
#include <thread>

#include <brynet/net/SocketLibFunction.h>
#include <brynet/net/fdset.h>

#include <brynet/net/Connector.h>

namespace brynet { namespace net {

    class AsyncConnectAddr
    {
    public:
        AsyncConnectAddr(std::string ip,
            int port,
            std::chrono::nanoseconds timeout,
            AsyncConnector::COMPLETED_CALLBACK successCB,
            AsyncConnector::FAILED_CALLBACK failedCB)
            :
            mIP(std::move(ip)),
            mPort(port),
            mTimeout(timeout),
            mSuccessCB(std::move(successCB)),
            mFailedCB(std::move(failedCB))
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
        const std::string                           mIP;
        const int                                   mPort;
        const std::chrono::nanoseconds              mTimeout;
        const AsyncConnector::COMPLETED_CALLBACK    mSuccessCB;
        const AsyncConnector::FAILED_CALLBACK       mFailedCB;
    };

    class ConnectorWorkInfo final : public utils::NonCopyable
    {
    public:
        typedef std::shared_ptr<ConnectorWorkInfo>    PTR;

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
            AsyncConnector::COMPLETED_CALLBACK      successCB;
            AsyncConnector::FAILED_CALLBACK         failedCB;
        };

        std::map<sock, ConnectingInfo>  mConnectingInfos;

        struct FDSetDeleter
        {
            void operator()(struct fdset_s* ptr) const
            {
                ox_fdset_delete(ptr);
            }
        };
        struct StackDeleter
        {
            void operator()(struct stack_s* ptr) const
            {
                ox_stack_delete(ptr);
            }
        };

        std::unique_ptr<struct fdset_s, FDSetDeleter> mFDSet;
        std::unique_ptr<struct stack_s, StackDeleter> mPollResult;
    };

    ConnectorWorkInfo::ConnectorWorkInfo() BRYNET_NOEXCEPT
    {
        mFDSet.reset(ox_fdset_new());
        mPollResult.reset(ox_stack_new(1024, sizeof(sock)));
    }

    bool ConnectorWorkInfo::isConnectSuccess(sock clientfd, bool willCheckWrite) const
    {
        if (willCheckWrite && !ox_fdset_check(mFDSet.get(), clientfd, WriteCheck))
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
        if (ox_fdset_poll(mFDSet.get(), millsecond) <= 0)
        {
            return;
        }

        std::set<sock>  total_fds;
        std::set<sock>  success_fds;

        ox_fdset_visitor(mFDSet.get(), WriteCheck, mPollResult.get());
        while (true)
        {
            auto p = ox_stack_popfront(mPollResult.get());
            if (p == nullptr)
            {
                break;
            }

            const sock fd = *(sock*)p;
            total_fds.insert(fd);
            if (isConnectSuccess(fd, false))
            {
                success_fds.insert(fd);
            }
        }

        for (auto fd : total_fds)
        {
            ox_fdset_remove(mFDSet.get(), fd);

            auto it = mConnectingInfos.find(fd);
            if (it == mConnectingInfos.end())
            {
                continue;
            }

            auto socket = TcpSocket::Create(fd, false);
            if (success_fds.find(fd) != success_fds.end())
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

            ox_fdset_remove(mFDSet.get(), fd);
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

            ox_fdset_remove(mFDSet.get(), fd);
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
        const int check_error = WSAEWOULDBLOCK;
#else
        const int check_error = EINPROGRESS;
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
            goto SUCCESS;
        }

        if (check_error != sErrno)
        {
            brynet::net::base::SocketClose(clientfd);
            clientfd = INVALID_SOCKET;
            goto FAILED;
        }

        {
            ConnectingInfo ci;
            ci.startConnectTime = std::chrono::steady_clock::now();
            ci.successCB = addr.getSuccessCB();
            ci.failedCB = addr.getFailedCB();
            ci.timeout = addr.getTimeout();

            mConnectingInfos[clientfd] = ci;
            ox_fdset_add(mFDSet.get(), clientfd, WriteCheck);
        }
        return;

    SUCCESS:
        if (addr.getSuccessCB() != nullptr)
        {
            addr.getSuccessCB()(TcpSocket::Create(clientfd, false));
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

        mEventLoop->pushAsyncProc([this]() {
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
        mEventLoop->pushAsyncProc([workInfo, address]() {
            workInfo->processConnect(address);
        });
    }

    AsyncConnector::PTR AsyncConnector::Create()
    {
        struct make_shared_enabler : public AsyncConnector {};
        return std::make_shared<make_shared_enabler>();
    }

} }