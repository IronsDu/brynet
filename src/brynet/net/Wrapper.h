#pragma once

#include <future>
#include <brynet/net/TcpConnection.h>
#include <brynet/net/TCPService.h>
#include <brynet/net/Connector.h>
#include <brynet/net/http/HttpService.h>

namespace brynet { namespace net { namespace wrapper {

    class SocketConnectBuilder
    {
    public:
        virtual ~SocketConnectBuilder() = default;
        
        SocketConnectBuilder& configureConnector(AsyncConnector::Ptr connector)
        {
            mConnector = std::move(connector);
            return *this;
        }

        SocketConnectBuilder& configureConnectOptions(
            std::vector<AsyncConnector::ConnectOptions::ConnectOptionFunc> options)
        {
            mConnectOptions = std::move(options);
            return *this;
        }

        void    asyncConnect() const
        {
            if (mConnector == nullptr)
            {
                throw std::runtime_error("connector is nullptr");
            }
            if (mConnectOptions.empty())
            {
                throw std::runtime_error("options is empty");
            }

            mConnector->asyncConnect(mConnectOptions);
        }

        TcpSocket::Ptr    syncConnect()
        {
            auto timeout = AsyncConnector::ConnectOptions::ExtractTimeout(mConnectOptions);

            auto socketPromise = std::make_shared<std::promise<TcpSocket::Ptr>>();
            mConnectOptions.push_back(AsyncConnector::ConnectOptions::WithCompletedCallback([socketPromise](TcpSocket::Ptr socket) {
                socketPromise->set_value(std::move(socket));
            }));
            mConnectOptions.push_back(AsyncConnector::ConnectOptions::WithFailedCallback([socketPromise]() {
                socketPromise->set_value(nullptr);
            }));

            asyncConnect();

            auto future = socketPromise->get_future();
            if (future.wait_for(timeout) != std::future_status::ready)
            {
                return nullptr;
            }

            return future.get();
        }

    private:
        AsyncConnector::Ptr                                             mConnector;
        std::vector<AsyncConnector::ConnectOptions::ConnectOptionFunc>  mConnectOptions;
    };

    class ConnectionBuilder
    {
    public:
        virtual ~ConnectionBuilder() = default;

        ConnectionBuilder& configureService(TcpService::Ptr service)
        {
            mTcpService = std::move(service);
            return *this;
        }

        ConnectionBuilder& configureConnector(AsyncConnector::Ptr connector)
        {
            mConnector = std::move(connector);
            return *this;
        }

        ConnectionBuilder& configureConnectOptions(std::vector<AsyncConnector::ConnectOptions::ConnectOptionFunc> options)
        {
            mConnectOptions = std::move(options);
            return *this;
        }

        ConnectionBuilder& configureConnectionOptions(std::vector<TcpService::AddSocketOption::AddSocketOptionFunc> options)
        {
            mConnectionOptions = std::move(options);
            return *this;
        }

        void    asyncConnect() const
        {
            if (mTcpService == nullptr || mConnector == nullptr)
            {
                throw std::runtime_error("tcp service or connector is nullptr");
            }
            if (mConnectOptions.empty() || mConnectionOptions.empty())
            {
                throw std::runtime_error("options is empty");
            }

            auto service = mTcpService;
            auto connectOptions = mConnectOptions;
            auto connectionOptions = mConnectionOptions;

            auto enterCallback = [=](TcpSocket::Ptr socket) mutable {
                service->addTcpConnection(std::move(socket), connectionOptions);
            };
            connectOptions.push_back(AsyncConnector::ConnectOptions::WithCompletedCallback(enterCallback));

            mConnector->asyncConnect(connectOptions);
        }

        TcpConnection::Ptr    syncConnect()
        {
            auto timeout = AsyncConnector::ConnectOptions::ExtractTimeout(mConnectOptions);
            auto sessionPromise = std::make_shared<std::promise<TcpConnection::Ptr>>();
            mConnectOptions.push_back(AsyncConnector::ConnectOptions::WithFailedCallback(
                [sessionPromise]() {
                sessionPromise->set_value(nullptr);
            }));

            mConnectionOptions.push_back(TcpService::AddSocketOption::AddEnterCallback(
                [sessionPromise](const TcpConnection::Ptr& session) {
                    sessionPromise->set_value(session);
                }));

            asyncConnect();

            auto future = sessionPromise->get_future();
            if (future.wait_for(timeout) != std::future_status::ready)
            {
                return nullptr;
            }

            return future.get();
        }

    protected:
        TcpService::Ptr                                                 mTcpService;
        AsyncConnector::Ptr                                             mConnector;
        std::vector<AsyncConnector::ConnectOptions::ConnectOptionFunc>  mConnectOptions;
        std::vector<TcpService::AddSocketOption::AddSocketOptionFunc>   mConnectionOptions;
    };

    class HttpConnectionBuilder
    {
    public:
        virtual ~HttpConnectionBuilder() = default;

        HttpConnectionBuilder& configureService(TcpService::Ptr service)
        {
            mTcpService = std::move(service);
            return *this;
        }

        HttpConnectionBuilder& configureConnector(AsyncConnector::Ptr connector)
        {
            mConnector = std::move(connector);
            return *this;
        }

        HttpConnectionBuilder& configureConnectOptions(std::vector<AsyncConnector::ConnectOptions::ConnectOptionFunc> options)
        {
            mConnectOptions = std::move(options);
            return *this;
        }

        HttpConnectionBuilder& configureConnectionOptions(std::vector<TcpService::AddSocketOption::AddSocketOptionFunc> options)
        {
            mConnectionOptions = std::move(options);
            return *this;
        }

        HttpConnectionBuilder& configureEnterCallback(http::HttpSession::EnterCallback callback)
        {
            mHttpEnterCallback = callback;
            return *this;
        }

        void    asyncConnect() const
        {
            if (mTcpService == nullptr || mConnector == nullptr)
            {
                throw std::runtime_error("tcp service or connector is nullptr");
            }
            if (mConnectOptions.empty() || mConnectionOptions.empty())
            {
                throw std::runtime_error("options is empty");
            }
            if (mHttpEnterCallback == nullptr)
            {
                throw std::runtime_error("not setting http enter callback");
            }

            auto service = mTcpService;
            auto connectOptions = mConnectOptions;
            auto connectionOptions = mConnectionOptions;
            auto callback = mHttpEnterCallback;
            connectionOptions.push_back(
                TcpService::AddSocketOption::AddEnterCallback([=](const TcpConnection::Ptr& session) {
                        http::HttpService::setup(session, callback);
                    }));

            auto enterCallback = [=](TcpSocket::Ptr socket) mutable {
                service->addTcpConnection(std::move(socket), connectionOptions);
            };
            connectOptions.push_back(AsyncConnector::ConnectOptions::WithCompletedCallback(enterCallback));

            mConnector->asyncConnect(connectOptions);
        }

    protected:
        TcpService::Ptr                                                 mTcpService;
        AsyncConnector::Ptr                                             mConnector;
        std::vector<AsyncConnector::ConnectOptions::ConnectOptionFunc>  mConnectOptions;
        std::vector<TcpService::AddSocketOption::AddSocketOptionFunc>   mConnectionOptions;
        http::HttpSession::EnterCallback                                mHttpEnterCallback;
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

    class ListenerBuilder
    {
    public:
        using ListenConfigSetting = std::function<void(BuildListenConfig)>;

        virtual ~ListenerBuilder() = default;

        ListenerBuilder& configureService(TcpService::Ptr service)
        {
            mTcpService = std::move(service);
            return *this;
        }

        ListenerBuilder& configureSocketOptions(std::vector<ListenThread::TcpSocketProcessCallback> options)
        {
            mSocketOptions = options;
            return *this;
        }

        ListenerBuilder& configureConnectionOptions(std::vector<TcpService::AddSocketOption::AddSocketOptionFunc> options)
        {
            mConnectionOptions = std::move(options);
            return *this;
        }

        ListenerBuilder& configureListen(ListenConfigSetting builder)
        {
            BuildListenConfig buildConfig(&mListenConfig);
            builder(buildConfig);
            return *this;
        }

        void asyncRun()
        {
            if (mTcpService == nullptr)
            {
                throw std::runtime_error("tcp service is nullptr");
            }
            if (mConnectionOptions.empty())
            {
                throw std::runtime_error("options is empty");
            }
            if (!mListenConfig.hasSetting())
            {
                throw std::runtime_error("not config listen addr");
            }

            auto service = mTcpService;
            auto connectionOptions = mConnectionOptions;
            mListenThread = ListenThread::Create(mListenConfig.useIpV6(),
                mListenConfig.ip(),
                mListenConfig.port(),
                [=](brynet::net::TcpSocket::Ptr socket) {
                    service->addTcpConnection(std::move(socket), connectionOptions);
                },
                mSocketOptions);
            mListenThread->startListen();
        }

        void    stop()
        {
            if (mListenThread)
            {
                mListenThread->stopListen();
            }
        }

    protected:
        TcpService::Ptr                                                 mTcpService;
        std::vector<TcpService::AddSocketOption::AddSocketOptionFunc>   mConnectionOptions;
        std::vector<ListenThread::TcpSocketProcessCallback>             mSocketOptions;
        ListenConfig                                                    mListenConfig;
        ListenThread::Ptr                                               mListenThread;
    };

    class HttpListenerBuilder
    {
    public:
        using ListenConfigSetting = std::function<void(BuildListenConfig)>;

        virtual ~HttpListenerBuilder() = default;

        HttpListenerBuilder& configureService(TcpService::Ptr service)
        {
            mTcpService = std::move(service);
            return *this;
        }

        HttpListenerBuilder& configureSocketOptions(std::vector<ListenThread::TcpSocketProcessCallback> options)
        {
            mSocketOptions = options;
            return *this;
        }

        HttpListenerBuilder& configureConnectionOptions(std::vector<TcpService::AddSocketOption::AddSocketOptionFunc> options)
        {
            mConnectionOptions = std::move(options);
            return *this;
        }

        HttpListenerBuilder& configureListen(ListenConfigSetting builder)
        {
            BuildListenConfig buildConfig(&mListenConfig);
            builder(buildConfig);
            return *this;
        }

        void asyncRun()
        {
            if (mTcpService == nullptr)
            {
                throw std::runtime_error("tcp service is nullptr");
            }
            if (mConnectionOptions.empty())
            {
                throw std::runtime_error("options is empty");
            }
            if (!mListenConfig.hasSetting())
            {
                throw std::runtime_error("not config listen addr");
            }
            if (mHttpEnterCallback == nullptr)
            {
                throw std::runtime_error("not setting http enter callback");
            }

            auto service = mTcpService;
            auto connectionOptions = mConnectionOptions;
            auto callback = mHttpEnterCallback;
            connectionOptions.push_back(
                TcpService::AddSocketOption::AddEnterCallback([=](const TcpConnection::Ptr& session) {
                    http::HttpService::setup(session, callback);
                }));

            mListenThread = ListenThread::Create(mListenConfig.useIpV6(),
                mListenConfig.ip(),
                mListenConfig.port(),
                [=](brynet::net::TcpSocket::Ptr socket) {
                    service->addTcpConnection(std::move(socket), connectionOptions);
                },
                mSocketOptions);
            mListenThread->startListen();
        }

        HttpListenerBuilder& configureEnterCallback(http::HttpSession::EnterCallback callback)
        {
            mHttpEnterCallback = callback;
            return *this;
        }

        void    stop()
        {
            if (mListenThread)
            {
                mListenThread->stopListen();
            }
        }

    protected:
        TcpService::Ptr                                                 mTcpService;
        std::vector<TcpService::AddSocketOption::AddSocketOptionFunc>   mConnectionOptions;
        std::vector<ListenThread::TcpSocketProcessCallback>             mSocketOptions;
        ListenConfig                                                    mListenConfig;
        ListenThread::Ptr                                               mListenThread;
        http::HttpSession::EnterCallback                                mHttpEnterCallback;
    };
} } }