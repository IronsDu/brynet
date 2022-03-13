#pragma once

#include <brynet/net/http/HttpService.hpp>
#include <brynet/net/wrapper/ConnectionBuilder.hpp>
#include <utility>

namespace brynet { namespace net { namespace wrapper {

class HttpConnectionBuilder
{
public:
    HttpConnectionBuilder& WithService(ITcpService::Ptr service)
    {
        mBuilder.WithService(std::move(service));
        return *this;
    }

    HttpConnectionBuilder& WithConnector(AsyncConnector::Ptr connector)
    {
        mBuilder.WithConnector(std::move(connector));
        return *this;
    }

    HttpConnectionBuilder& WithAddr(std::string ip, size_t port)
    {
        mBuilder.WithAddr(std::move(ip), port);
        return *this;
    }

    HttpConnectionBuilder& WithTimeout(std::chrono::nanoseconds timeout)
    {
        mBuilder.WithTimeout(timeout);
        return *this;
    }

    HttpConnectionBuilder& AddSocketProcessCallback(const ProcessTcpSocketCallback& callback)
    {
        mBuilder.AddSocketProcessCallback(callback);
        return *this;
    }

    HttpConnectionBuilder& WithEnterCallback(http::HttpSession::EnterCallback&& callback)
    {
        mHttpEnterCallback = std::move(callback);
        return *this;
    }

    HttpConnectionBuilder& WithFailedCallback(FailedCallback callback)
    {
        mBuilder.WithFailedCallback(std::move(callback));
        return *this;
    }

    HttpConnectionBuilder& WithMaxRecvBufferSize(size_t size)
    {
        mBuilder.WithMaxRecvBufferSize(size);
        return *this;
    }

#ifdef BRYNET_USE_OPENSSL
    HttpConnectionBuilder& WithSSL()
    {
        mBuilder.WithSSL();
        return *this;
    }
#endif
    HttpConnectionBuilder& WithForceSameThreadLoop()
    {
        mBuilder.WithForceSameThreadLoop();
        return *this;
    }

    void asyncConnect()
    {
        if (mHttpEnterCallback == nullptr)
        {
            throw BrynetCommonException("not setting http enter callback");
        }

        auto callback = mHttpEnterCallback;
        auto builder = mBuilder;
        builder.AddEnterCallback([callback](const TcpConnection::Ptr& session) {
            http::HttpService::setup(session, callback);
        });
        builder.asyncConnect();
    }

private:
    http::HttpSession::EnterCallback mHttpEnterCallback;
    ConnectionBuilder mBuilder;
};

}}}// namespace brynet::net::wrapper
