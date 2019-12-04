#pragma once

#include <future>
#include <brynet/net/TcpService.hpp>
#include <brynet/net/AsyncConnector.hpp>
#include <brynet/net/Exception.hpp>

namespace brynet { namespace net { namespace wrapper {

    template<typename Derived>
    class BaseSocketConnectBuilder
    {
    protected:
        using AddSocketOptionFunc = detail::AddSocketOptionFunc;
        using ConnectOptionFunc = detail::ConnectOptionFunc;

    public:
        virtual ~BaseSocketConnectBuilder() = default;

        Derived& configureConnector(AsyncConnector::Ptr connector)
        {
            mConnector = std::move(connector);
            return static_cast<Derived&>(*this);
        }

        Derived& configureConnectOptions(
            std::vector<ConnectOptionFunc> options)
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
        void    asyncConnect(std::vector<ConnectOptionFunc> connectOptions) const
        {
            if (mConnector == nullptr)
            {
                throw BrynetCommonException("connector is nullptr");
            }
            if (connectOptions.empty())
            {
                throw BrynetCommonException("options is empty");
            }

            mConnector->asyncConnect(connectOptions);
        }

        TcpSocket::Ptr  syncConnect(std::vector<ConnectOptionFunc> connectOptions) const
        {
            auto timeout = ConnectOption::ExtractTimeout(connectOptions);

            auto socketPromise = std::make_shared<std::promise<TcpSocket::Ptr>>();
            connectOptions.push_back(ConnectOption::WithCompletedCallback(
                [socketPromise](TcpSocket::Ptr socket) {
                    socketPromise->set_value(std::move(socket));
                }));
            connectOptions.push_back(ConnectOption::WithFailedCallback([socketPromise]() {
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

        std::vector<ConnectOptionFunc>  getConnectOptions() const
        {
            return mConnectOptions;
        }

    private:
        AsyncConnector::Ptr             mConnector;
        std::vector<ConnectOptionFunc>  mConnectOptions;
    };

    class SocketConnectBuilder : public BaseSocketConnectBuilder<SocketConnectBuilder>
    {
    };

    template<typename Derived>
    class BaseConnectionBuilder : public BaseSocketConnectBuilder<Derived>
    {
    protected:
        using AddSocketOptionFunc = detail::AddSocketOptionFunc;
        using ConnectOptionFunc = detail::ConnectOptionFunc;

    public:
        Derived& configureService(TcpService::Ptr service)
        {
            mTcpService = std::move(service);
            return static_cast<Derived&>(*this);
        }

        Derived& configureConnectionOptions(std::vector<AddSocketOptionFunc> options)
        {
            mConnectionOptions = std::move(options);
            return static_cast<Derived&>(*this);
        }

        void    asyncConnect() const
        {
            asyncConnect(BaseSocketConnectBuilder<Derived>::getConnectOptions(), 
                mConnectionOptions);
        }

        TcpConnection::Ptr    syncConnect() const
        {
            return syncConnect(BaseSocketConnectBuilder<Derived>::getConnectOptions(), 
                mConnectionOptions);
        }

    protected:
        void    asyncConnect(std::vector<ConnectOptionFunc> connectOptions,
            std::vector<AddSocketOptionFunc> connectionOptions) const
        {
            if (mTcpService == nullptr)
            {
                throw BrynetCommonException("tcp serviceis nullptr");
            }
            if (connectionOptions.empty())
            {
                throw BrynetCommonException("options is empty");
            }

            auto service = mTcpService;
            auto enterCallback = [service, connectionOptions](TcpSocket::Ptr socket) mutable {
                service->addTcpConnection(std::move(socket), connectionOptions);
            };
            connectOptions.push_back(ConnectOption::WithCompletedCallback(enterCallback));

            BaseSocketConnectBuilder<Derived>::asyncConnect(connectOptions);
        }

        TcpConnection::Ptr  syncConnect(std::vector<ConnectOptionFunc> connectOptions,
            std::vector<AddSocketOptionFunc> connectionOptions) const
        {
            auto timeout = ConnectOption::ExtractTimeout(connectOptions);
            auto sessionPromise = std::make_shared<std::promise<TcpConnection::Ptr>>();

            connectOptions.push_back(ConnectOption::WithFailedCallback(
                [sessionPromise]() {
                    sessionPromise->set_value(nullptr);
                }));

            connectionOptions.push_back(AddSocketOption::AddEnterCallback(
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

        std::vector<AddSocketOptionFunc>   getConnectionOptions() const
        {
            return mConnectionOptions;
        }

    private:
        TcpService::Ptr                     mTcpService;
        std::vector<AddSocketOptionFunc>    mConnectionOptions;
    };

    class ConnectionBuilder : public BaseConnectionBuilder<ConnectionBuilder>
    {
    };

} } }