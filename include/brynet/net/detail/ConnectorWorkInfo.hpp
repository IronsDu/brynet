#pragma once

#include <functional>
#include <memory>

#include <brynet/base/NonCopyable.hpp>
#include <brynet/base/CPP_VERSION.hpp>
#include <brynet/net/SocketLibTypes.hpp>
#include <brynet/net/Socket.hpp>
#include <brynet/net/Poller.hpp>

#ifdef BRYNET_HAVE_LANG_CXX17
#include <shared_mutex>
#else
#include <mutex>
#endif

namespace brynet { namespace net { namespace detail {

    class ConnectOptionsInfo;
    using ConnectOptionFunc = std::function<void(ConnectOptionsInfo & option)>;

    class AsyncConnectAddr final
    {
    public:
        using CompletedCallback = std::function<void(TcpSocket::Ptr)>;
        using ProcessTcpSocketCallback = std::function<void(TcpSocket&)>;
        using FailedCallback = std::function<void()>;

    public:
        AsyncConnectAddr(std::string&& ip,
            int port,
            std::chrono::nanoseconds timeout,
            CompletedCallback&& successCB,
            FailedCallback&& failedCB,
            std::vector<ProcessTcpSocketCallback>&& processCallbacks)
            :
            mIP(std::move(ip)),
            mPort(port),
            mTimeout(timeout),
            mSuccessCB(std::move(successCB)),
            mFailedCB(std::move(failedCB)),
            mProcessCallbacks(std::move(processCallbacks))
        {
        }

        const std::string& getIP() const
        {
            return mIP;
        }

        int                         getPort() const
        {
            return mPort;
        }

        const CompletedCallback&    getSuccessCB() const
        {
            return mSuccessCB;
        }

        const FailedCallback&       getFailedCB() const
        {
            return mFailedCB;
        }

        const std::vector<ProcessTcpSocketCallback>& getProcessCallbacks() const
        {
            return mProcessCallbacks;
        }

        std::chrono::nanoseconds    getTimeout() const
        {
            return mTimeout;
        }

    private:
        const std::string                           mIP;
        const int                                   mPort;
        const std::chrono::nanoseconds              mTimeout;
        const CompletedCallback                     mSuccessCB;
        const FailedCallback                        mFailedCB;
        const std::vector<ProcessTcpSocketCallback> mProcessCallbacks;
        };

    class ConnectorWorkInfo final : public brynet::base::NonCopyable
    {
    public:
        using Ptr = std::shared_ptr<ConnectorWorkInfo>;

        ConnectorWorkInfo() BRYNET_NOEXCEPT
        {
            mPoller.reset(brynet::base::poller_new());
            mPollResult.reset(brynet::base::stack_new(1024, sizeof(BrynetSocketFD)));
        }

        void    checkConnectStatus(int millsecond)
        {
            if (poller_poll(mPoller.get(), millsecond) <= 0)
            {
                return;
            }

            std::set<BrynetSocketFD>  totalFds;
            std::set<BrynetSocketFD>  successFds;

            poller_visitor(mPoller.get(), brynet::base::WriteCheck, mPollResult.get());
            while (true)
            {
                auto p = stack_popfront(mPollResult.get());
                if (p == nullptr)
                {
                    break;
                }

                const auto fd = *(BrynetSocketFD*)p;
                totalFds.insert(fd);
                if (isConnectSuccess(fd, false) && 
                    !brynet::net::base::IsSelfConnect(fd))
                {
                    successFds.insert(fd);
                }
            }

            for (auto fd : totalFds)
            {
                poller_remove(mPoller.get(), fd);

                const auto it = mConnectingInfos.find(fd);
                if (it == mConnectingInfos.end())
                {
                    continue;
                }

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

                mConnectingInfos.erase(it);
            }
        }

        bool    isConnectSuccess(BrynetSocketFD clientfd, bool willCheckWrite) const
        {
            if (willCheckWrite && !poller_check(mPoller.get(), clientfd, brynet::base::WriteCheck))
            {
                return false;
            }

            int error = BRYNET_SOCKET_ERROR;
            int len = sizeof(error);
            if (getsockopt(clientfd, 
                SOL_SOCKET, 
                SO_ERROR, 
                (char*)&error,
                (socklen_t*)&len) == BRYNET_SOCKET_ERROR)
            {
                return false;
            }

            return error == 0;
        }

        void    checkTimeout()
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

                poller_remove(mPoller.get(), fd);
                mConnectingInfos.erase(it++);

                brynet::net::base::SocketClose(fd);
                if (cb != nullptr)
                {
                    //TODO::don't modify mConnectingInfos in cb
                    cb();
                }
            }
        }

        void    processConnect(const AsyncConnectAddr& addr)
        {
            struct sockaddr_in server_addr = sockaddr_in();
            BrynetSocketFD clientfd = BRYNET_INVALID_SOCKET;

#ifdef BRYNET_PLATFORM_WINDOWS
            const int ExpectedError = WSAEWOULDBLOCK;
#else
            const int ExpectedError = EINPROGRESS;
#endif
            int n = 0;

            brynet::net::base::InitSocket();

            clientfd = brynet::net::base::SocketCreate(AF_INET, SOCK_STREAM, 0);
            if (clientfd == BRYNET_INVALID_SOCKET)
            {
                goto FAILED;
            }

            brynet::net::base::SocketNonblock(clientfd);
            server_addr.sin_family = AF_INET;
            inet_pton(AF_INET, addr.getIP().c_str(), &server_addr.sin_addr.s_addr);
            server_addr.sin_port = static_cast<decltype(server_addr.sin_port)>(htons(addr.getPort()));

            n = connect(clientfd, (struct sockaddr*) & server_addr, sizeof(struct sockaddr));
            if (n == 0)
            {
                if (brynet::net::base::IsSelfConnect(clientfd))
                {
                    goto FAILED;
                }
            }
            else if (BRYNET_ERRNO != ExpectedError)
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
                poller_add(mPoller.get(), clientfd, brynet::base::WriteCheck);

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
            if (clientfd != BRYNET_INVALID_SOCKET)
            {
                brynet::net::base::SocketClose(clientfd);
                clientfd = BRYNET_INVALID_SOCKET;
                (void)clientfd;
            }
            if (addr.getFailedCB() != nullptr)
            {
                addr.getFailedCB()();
            }
        }

        void    causeAllFailed()
        {
            auto copyMap = mConnectingInfos;
            mConnectingInfos.clear();

            for (const auto& v : copyMap)
            {
                auto fd = v.first;
                auto cb = v.second.failedCB;

                poller_remove(mPoller.get(), fd);
                brynet::net::base::SocketClose(fd);
                if (cb != nullptr)
                {
                    cb();
                }
            }
        }

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
            AsyncConnectAddr::CompletedCallback                       successCB;
            AsyncConnectAddr::FailedCallback                          failedCB;
            std::vector<AsyncConnectAddr::ProcessTcpSocketCallback>   processCallbacks;
        };

        std::map<BrynetSocketFD, ConnectingInfo>  mConnectingInfos;

        class PollerDeleter
        {
        public:
            void operator()(struct brynet::base::poller_s* ptr) const
            {
                brynet::base::poller_delete(ptr);
            }
        };
        class StackDeleter
        {
        public:
            void operator()(struct brynet::base::stack_s* ptr) const
            {
                brynet::base::stack_delete(ptr);
            }
        };

        std::unique_ptr<struct brynet::base::poller_s, PollerDeleter> mPoller;
        std::unique_ptr<struct brynet::base::stack_s, StackDeleter>   mPollResult;
    };

    static void RunOnceCheckConnect(
        const std::shared_ptr<brynet::net::EventLoop>& eventLoop,
        const std::shared_ptr<ConnectorWorkInfo>& workerInfo)
    {
        eventLoop->loop(std::chrono::milliseconds(10).count());
        workerInfo->checkConnectStatus(0);
        workerInfo->checkTimeout();
    }

    class ConnectOptionsInfo final
    {
    public:
        ConnectOptionsInfo()
            :
            port(0),
            timeout(std::chrono::seconds(10))
        {
        }

        std::string ip;
        int port;
        std::chrono::nanoseconds timeout;
        std::vector<AsyncConnectAddr::ProcessTcpSocketCallback> processCallbacks;
        AsyncConnectAddr::CompletedCallback completedCallback;
        AsyncConnectAddr::FailedCallback faledCallback;
    };

} } }