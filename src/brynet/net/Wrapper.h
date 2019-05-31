#pragma once

#include <future>
#include <brynet/net/TcpConnection.h>
#include <brynet/net/TCPService.h>
#include <brynet/net/Connector.h>
#include <brynet/net/http/HttpService.h>

namespace brynet { namespace net { namespace wrapper {

    template<typename Derived>
    class BaseSocketConnectBuilder
    {
    public:
        virtual ~BaseSocketConnectBuilder() = default;

        Derived& configureConnector(AsyncConnector::Ptr connector)
        {
            mConnector = std::move(connector);
            return static_cast<Derived&>(*this);
        }

        Derived& configureConnectOptions(
            std::vector<AsyncConnector::ConnectOptions::ConnectOptionFunc> options)
        {
            mConnectOptions = std::move(options);
            return static_cast<Derived&>(*this);
        }

        void    asyncConnect() const
        {
            asyncConnect(mConnectOptions);
        }

        TcpSocket::Ptr    syncConnect() const
        {
            return syncConnect(mConnectOptions);
        }

    protected:
        void    asyncConnect(std::vector<AsyncConnector::ConnectOptions::ConnectOptionFunc> connectOptions) const
        {
            if (mConnector == nullptr)
            {
                throw std::runtime_error("connector is nullptr");
            }
            if (connectOptions.empty())
            {
                throw std::runtime_error("options is empty");
            }

            mConnector->asyncConnect(connectOptions);
        }

        TcpSocket::Ptr  syncConnect(std::vector<AsyncConnector::ConnectOptions::ConnectOptionFunc> connectOptions) const
        {
            auto timeout = AsyncConnector::ConnectOptions::ExtractTimeout(connectOptions);

            auto socketPromise = std::make_shared<std::promise<TcpSocket::Ptr>>();
            connectOptions.push_back(AsyncConnector::ConnectOptions::WithCompletedCallback([socketPromise](TcpSocket::Ptr socket) {
                    socketPromise->set_value(std::move(socket));
                }));
            connectOptions.push_back(AsyncConnector::ConnectOptions::WithFailedCallback([socketPromise]() {
                    socketPromise->set_value(nullptr);
                }));

            asyncConnect(connectOptions);

            auto future = socketPromise->get_future();
            if (future.wait_for(timeout) != std::future_status::ready)
            {
                return nullptr;
            }

            return future.get();
        }

        std::vector<AsyncConnector::ConnectOptions::ConnectOptionFunc>  getConnectOptions() const
        {
            return mConnectOptions;
        }

    private:
        AsyncConnector::Ptr                                             mConnector;
        std::vector<AsyncConnector::ConnectOptions::ConnectOptionFunc>  mConnectOptions;
    };

    class SocketConnectBuilder : public BaseSocketConnectBuilder<SocketConnectBuilder>
    {
    };

    template<typename Derived>
    class BaseConnectionBuilder : public BaseSocketConnectBuilder<Derived>
    {
    public:
        Derived& configureService(TcpService::Ptr service)
        {
            mTcpService = std::move(service);
            return static_cast<Derived&>(*this);
        }

        Derived& configureConnectionOptions(std::vector<TcpService::AddSocketOption::AddSocketOptionFunc> options)
        {
            mConnectionOptions = std::move(options);
            return static_cast<Derived&>(*this);
        }

        void    asyncConnect() const
        {
            asyncConnect(BaseSocketConnectBuilder<Derived>::getConnectOptions(), mConnectionOptions);
        }

        TcpConnection::Ptr    syncConnect() const
        {
            return syncConnect(BaseSocketConnectBuilder<Derived>::getConnectOptions(), mConnectionOptions);
        }

    protected:
        void    asyncConnect(std::vector<AsyncConnector::ConnectOptions::ConnectOptionFunc> connectOptions,
            std::vector<TcpService::AddSocketOption::AddSocketOptionFunc> connectionOptions) const
        {
            if (mTcpService == nullptr)
            {
                throw std::runtime_error("tcp serviceis nullptr");
            }
            if (connectionOptions.empty())
            {
                throw std::runtime_error("options is empty");
            }

            auto service = mTcpService;
            auto enterCallback = [=](TcpSocket::Ptr socket) mutable {
                service->addTcpConnection(std::move(socket), connectionOptions);
            };
            connectOptions.push_back(AsyncConnector::ConnectOptions::WithCompletedCallback(enterCallback));

            BaseSocketConnectBuilder<Derived>::asyncConnect(connectOptions);
        }

        TcpConnection::Ptr  syncConnect(std::vector<AsyncConnector::ConnectOptions::ConnectOptionFunc> connectOptions,
            std::vector<TcpService::AddSocketOption::AddSocketOptionFunc> connectionOptions) const
        {
            auto timeout = AsyncConnector::ConnectOptions::ExtractTimeout(connectOptions);
            auto sessionPromise = std::make_shared<std::promise<TcpConnection::Ptr>>();

            connectOptions.push_back(AsyncConnector::ConnectOptions::WithFailedCallback(
                [sessionPromise]() {
                    sessionPromise->set_value(nullptr);
                }));

            connectionOptions.push_back(TcpService::AddSocketOption::AddEnterCallback(
                [sessionPromise](const TcpConnection::Ptr& session) {
                    sessionPromise->set_value(session);
                }));

            asyncConnect(connectOptions, connectionOptions);

            auto future = sessionPromise->get_future();
            if (future.wait_for(timeout) != std::future_status::ready)
            {
                return nullptr;
            }

            return future.get();
        }

        std::vector<TcpService::AddSocketOption::AddSocketOptionFunc>   getConnectionOptions() const
        {
            return mConnectionOptions;
        }

    private:
        TcpService::Ptr                                                 mTcpService;
        std::vector<TcpService::AddSocketOption::AddSocketOptionFunc>   mConnectionOptions;
    };

    class ConnectionBuilder : public BaseConnectionBuilder<ConnectionBuilder>
    {
    };

    class HttpConnectionBuilder : public BaseConnectionBuilder<HttpConnectionBuilder>
    {
    public:
        HttpConnectionBuilder& configureEnterCallback(http::HttpSession::EnterCallback callback)
        {
            mHttpEnterCallback = callback;
            return *this;
        }

        void    asyncConnect() const
        {
            if (mHttpEnterCallback == nullptr)
            {
                throw std::runtime_error("not setting http enter callback");
            }

            auto connectionOptions = BaseConnectionBuilder<HttpConnectionBuilder>::getConnectionOptions();
            auto callback = mHttpEnterCallback;

            connectionOptions.push_back(
                TcpService::AddSocketOption::AddEnterCallback([=](const TcpConnection::Ptr& session) {
                        http::HttpService::setup(session, callback);
                    }));

            BaseConnectionBuilder<HttpConnectionBuilder>::asyncConnect(BaseConnectionBuilder<HttpConnectionBuilder>::getConnectOptions(), connectionOptions);
        }

    protected:
        http::HttpSession::EnterCallback    mHttpEnterCallback;
    };

    class ListenConfig final
    {
    public:
        ListenConfig()
        {
            mSetting = false;
            mIsIpV6 = false;
        }

        void        setAddr(bool ipV6, std::string ip, int port)
        {
            mIsIpV6 = ipV6;
            mListenAddr = ip;
            mPort = port;
            mSetting = true;
        }

        std::string ip() const
        {
            return mListenAddr;
        }

        int         port() const
        {
            return mPort;
        }

        bool        useIpV6() const
        {
            return mIsIpV6;
        }

        bool        hasSetting() const
        {
            return mSetting;
        }

    private:
        std::string mListenAddr;
        int         mPort;
        bool        mIsIpV6;
        bool        mSetting;
    };

    class BuildListenConfig
    {
    public:
        BuildListenConfig(ListenConfig* config)
        {
            mConfig = config;
        }

        void        setAddr(bool ipV6, std::string ip, int port)
        {
            mConfig->setAddr(ipV6, ip, port);
        }
    private:
        ListenConfig* mConfig;
    };

    template<typename Derived>
    class BaseListenerBuilder
    {
    public:
        using ListenConfigSetting = std::function<void(BuildListenConfig)>;

        virtual ~BaseListenerBuilder() = default;

        Derived& configureService(TcpService::Ptr service)
        {
            mTcpService = std::move(service);
            return static_cast<Derived&>(*this);
        }

        Derived& configureSocketOptions(std::vector<ListenThread::TcpSocketProcessCallback> options)
        {
            mSocketOptions = std::move(options);
            return static_cast<Derived&>(*this);
        }

        Derived& configureConnectionOptions(std::vector<TcpService::AddSocketOption::AddSocketOptionFunc> options)
        {
            mConnectionOptions = std::move(options);
            return static_cast<Derived&>(*this);
        }

        Derived& configureListen(ListenConfigSetting builder)
        {
            BuildListenConfig buildConfig(&mListenConfig);
            builder(buildConfig);
            return static_cast<Derived&>(*this);
        }

        void asyncRun()
        {
            asyncRun(getConnectionOptions());
        }

        void    stop()
        {
            if (mListenThread)
            {
                mListenThread->stopListen();
            }
        }

        std::vector<TcpService::AddSocketOption::AddSocketOptionFunc>   getConnectionOptions() const
        {
            return mConnectionOptions;
        }

    protected:
        void asyncRun(std::vector<TcpService::AddSocketOption::AddSocketOptionFunc> connectionOptions)
        {
            if (mTcpService == nullptr)
            {
                throw std::runtime_error("tcp service is nullptr");
            }
            if (connectionOptions.empty())
            {
                throw std::runtime_error("options is empty");
            }
            if (!mListenConfig.hasSetting())
            {
                throw std::runtime_error("not config listen addr");
            }

            auto service = mTcpService;
            mListenThread = ListenThread::Create(mListenConfig.useIpV6(),
                mListenConfig.ip(),
                mListenConfig.port(),
                [=](brynet::net::TcpSocket::Ptr socket) {
                    service->addTcpConnection(std::move(socket), connectionOptions);
                },
                mSocketOptions);
            mListenThread->startListen();
        }

    private:
        TcpService::Ptr                                                 mTcpService;
        std::vector<ListenThread::TcpSocketProcessCallback>             mSocketOptions;
        ListenConfig                                                    mListenConfig;
        ListenThread::Ptr                                               mListenThread;

    private:
        std::vector<TcpService::AddSocketOption::AddSocketOptionFunc>   mConnectionOptions;
    };

    class ListenerBuilder : public BaseListenerBuilder<ListenerBuilder>
    {
    };

    class HttpListenerBuilder : public BaseListenerBuilder<HttpListenerBuilder>
    {
    public:
        HttpListenerBuilder& configureEnterCallback(http::HttpSession::EnterCallback callback)
        {
            mHttpEnterCallback = callback;
            return *this;
        }

        void asyncRun()
        {
            if (mHttpEnterCallback == nullptr)
            {
                throw std::runtime_error("not setting http enter callback");
            }

            auto connectionOptions = BaseListenerBuilder<HttpListenerBuilder>::getConnectionOptions();
            auto callback = mHttpEnterCallback;
            connectionOptions.push_back(
                TcpService::AddSocketOption::AddEnterCallback([=](const TcpConnection::Ptr& session) {
                    http::HttpService::setup(session, callback);
                }));
            BaseListenerBuilder<HttpListenerBuilder>::asyncRun(connectionOptions);
        }

    protected:
        http::HttpSession::EnterCallback                                mHttpEnterCallback;
    };

} } }