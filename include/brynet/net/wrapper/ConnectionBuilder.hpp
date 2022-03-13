#pragma once

#include <brynet/net/AsyncConnector.hpp>
#include <brynet/net/Exception.hpp>
#include <brynet/net/TcpService.hpp>
#include <future>
#include <utility>

namespace brynet { namespace net { namespace wrapper {

using CompletedCallback = detail::AsyncConnectAddr::CompletedCallback;
using ProcessTcpSocketCallback = detail::AsyncConnectAddr::ProcessTcpSocketCallback;
using FailedCallback = detail::AsyncConnectAddr::FailedCallback;

template<typename Derived>
class BaseSocketConnectBuilder
{
public:
    virtual ~BaseSocketConnectBuilder() = default;

    Derived& WithConnector(AsyncConnector::Ptr connector)
    {
        mConnector = std::move(connector);
        return static_cast<Derived&>(*this);
    }

    Derived& WithAddr(std::string ip, size_t port)
    {
        mConnectOption.ip = std::move(ip);
        mConnectOption.port = port;
        return static_cast<Derived&>(*this);
    }

    Derived& WithTimeout(std::chrono::nanoseconds timeout)
    {
        mConnectOption.timeout = timeout;
        return static_cast<Derived&>(*this);
    }

    Derived& AddSocketProcessCallback(const ProcessTcpSocketCallback& callback)
    {
        mConnectOption.processCallbacks.push_back(callback);
        return static_cast<Derived&>(*this);
    }

    Derived& WithCompletedCallback(CompletedCallback callback)
    {
        mConnectOption.completedCallback = std::move(callback);
        return static_cast<Derived&>(*this);
    }

    Derived& WithFailedCallback(FailedCallback callback)
    {
        mConnectOption.failedCallback = std::move(callback);
        return static_cast<Derived&>(*this);
    }

    void asyncConnect() const
    {
        if (mConnector == nullptr)
        {
            throw BrynetCommonException("connector is nullptr");
        }
        if (mConnectOption.ip.empty())
        {
            throw BrynetCommonException("address is empty");
        }

        mConnector->asyncConnect(mConnectOption);
    }

    TcpSocket::Ptr syncConnect()
    {
        if (mConnectOption.completedCallback != nullptr || mConnectOption.failedCallback != nullptr)
        {
            throw std::runtime_error("already setting completed callback or failed callback");
        }

        auto socketPromise = std::make_shared<std::promise<TcpSocket::Ptr>>();
        mConnectOption.completedCallback = [socketPromise](TcpSocket::Ptr socket) {
            socketPromise->set_value(std::move(socket));
        };
        mConnectOption.failedCallback = [socketPromise]() {
            socketPromise->set_value(nullptr);
        };

        asyncConnect();

        auto future = socketPromise->get_future();
        if (future.wait_for(mConnectOption.timeout) != std::future_status::ready)
        {
            return nullptr;
        }

        return future.get();
    }

private:
    AsyncConnector::Ptr mConnector;
    ConnectOption mConnectOption;
};

class SocketConnectBuilder : public BaseSocketConnectBuilder<SocketConnectBuilder>
{
};

template<typename Derived>
class BaseConnectionBuilder
{
public:
    Derived& WithService(ITcpService::Ptr service)
    {
        mTcpService = std::move(service);
        return static_cast<Derived&>(*this);
    }

    Derived& WithConnector(AsyncConnector::Ptr connector)
    {
        mConnectBuilder.WithConnector(std::move(connector));
        return static_cast<Derived&>(*this);
    }

    Derived& WithAddr(std::string ip, size_t port)
    {
        mConnectBuilder.WithAddr(std::move(ip), port);
        return static_cast<Derived&>(*this);
    }

    Derived& WithTimeout(std::chrono::nanoseconds timeout)
    {
        mConnectBuilder.WithTimeout(timeout);
        return static_cast<Derived&>(*this);
    }

    Derived& AddSocketProcessCallback(const ProcessTcpSocketCallback& callback)
    {
        mConnectBuilder.AddSocketProcessCallback(callback);
        return static_cast<Derived&>(*this);
    }

    Derived& WithFailedCallback(FailedCallback callback)
    {
        mConnectBuilder.WithFailedCallback(std::move(callback));
        return static_cast<Derived&>(*this);
    }

    Derived& WithMaxRecvBufferSize(size_t size)
    {
        mOption.maxRecvBufferSize = size;
        return static_cast<Derived&>(*this);
    }

#ifdef BRYNET_USE_OPENSSL
    Derived& WithSSL()
    {
        mOption.useSSL = true;
        return static_cast<Derived&>(*this);
    }
#endif
    Derived& WithForceSameThreadLoop()
    {
        mOption.forceSameThreadLoop = true;
        return static_cast<Derived&>(*this);
    }

    Derived& AddEnterCallback(const TcpConnection::EnterCallback& callback)
    {
        mOption.enterCallback.push_back(callback);
        return static_cast<Derived&>(*this);
    }

    void asyncConnect()
    {
        auto service = mTcpService;
        auto option = mOption;
        mConnectBuilder.WithCompletedCallback([service, option](TcpSocket::Ptr socket) mutable {
            service->addTcpConnection(std::move(socket), option);
        });

        mConnectBuilder.asyncConnect();
    }

    TcpConnection::Ptr syncConnect()
    {
        auto sessionPromise = std::make_shared<std::promise<TcpConnection::Ptr>>();

        auto option = mOption;
        option.enterCallback.push_back([sessionPromise](const TcpConnection::Ptr& session) {
            sessionPromise->set_value(session);
        });

        auto socket = mConnectBuilder.syncConnect();
        if (socket == nullptr || !mTcpService->addTcpConnection(std::move(socket), option))
        {
            return nullptr;
        }
        return sessionPromise->get_future().get();
    }

private:
    ITcpService::Ptr mTcpService;
    ConnectionOption mOption;
    SocketConnectBuilder mConnectBuilder;
};

class ConnectionBuilder : public BaseConnectionBuilder<ConnectionBuilder>
{
};

}}}// namespace brynet::net::wrapper
