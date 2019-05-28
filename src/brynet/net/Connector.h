#pragma once

#include <functional>
#include <memory>

#include <brynet/net/EventLoop.h>
#include <brynet/net/SocketLibTypes.h>
#include <brynet/utils/NonCopyable.h>
#include <brynet/utils/CPP_VERSION.h>
#include <brynet/net/Any.h>
#include <brynet/net/Noexcept.h>
#include <brynet/net/Socket.h>

#ifdef HAVE_LANG_CXX17
#include <shared_mutex>
#else
#include <mutex>
#endif

namespace brynet { namespace net {

    class ConnectorWorkInfo;

    class AsyncConnector : utils::NonCopyable, public std::enable_shared_from_this<AsyncConnector>
    {
    public:
        using Ptr = std::shared_ptr<AsyncConnector>;
        using CompletedCallback = std::function<void(TcpSocket::Ptr)>;
        using ProcessTcpSocketCallback = std::function<void(TcpSocket&)>;
        using FailedCallback = std::function<void()>;

        class ConnectOptions
        {
        public:
            struct Options;

            using ConnectOptionFunc = std::function<void(Options& option)>;

            static ConnectOptionFunc WithAddr(const std::string& ip, int port);
            static ConnectOptionFunc WithTimeout(std::chrono::nanoseconds timeout);
            static ConnectOptionFunc WithCompletedCallback(CompletedCallback callback);
            static ConnectOptionFunc AddProcessTcpSocketCallback(ProcessTcpSocketCallback process);
            static ConnectOptionFunc WithFailedCallback(FailedCallback callback);

            static std::chrono::nanoseconds ExtractTimeout(const std::vector<ConnectOptions::ConnectOptionFunc>& options);
        };

        void                                startWorkerThread();
        void                                stopWorkerThread();
        void                                asyncConnect(const std::vector<ConnectOptions::ConnectOptionFunc>& options);

        static  Ptr                         Create();

    private:
        AsyncConnector();
        virtual ~AsyncConnector();

    private:
        std::shared_ptr<EventLoop>          mEventLoop;

        std::shared_ptr<ConnectorWorkInfo>  mWorkInfo;
        std::shared_ptr<std::thread>        mThread;
#ifdef HAVE_LANG_CXX17
        std::shared_mutex                   mThreadGuard;
#else
        std::mutex                          mThreadGuard;
#endif
        std::shared_ptr<bool>               mIsRun;
    };

} }