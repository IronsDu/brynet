#pragma once

#include <brynet/net/http/HttpService.hpp>
#include <brynet/net/wrapper/ServiceBuilder.hpp>
#include <utility>

namespace brynet { namespace net { namespace wrapper {

class HttpListenerBuilder
{
public:
    HttpListenerBuilder& WithService(TcpService::Ptr service)
    {
        mBuilder.WithService(std::move(service));
        return *this;
    }

    HttpListenerBuilder& WithEnterCallback(http::HttpSession::EnterCallback&& callback)
    {
        mHttpEnterCallback = std::move(callback);
        return *this;
    }

    HttpListenerBuilder& AddSocketProcess(const ListenThread::TcpSocketProcessCallback& callback)
    {
        mBuilder.AddSocketProcess(callback);
        return *this;
    }

    HttpListenerBuilder& WithMaxRecvBufferSize(size_t size)
    {
        mBuilder.WithMaxRecvBufferSize(size);
        return *this;
    }
#ifdef BRYNET_USE_OPENSSL
    HttpListenerBuilder& WithSSL(SSLHelper::Ptr sslHelper)
    {
        mBuilder.WithSSL(std::move(sslHelper));
        return *this;
    }
#endif
    HttpListenerBuilder& WithForceSameThreadLoop()
    {
        mBuilder.WithForceSameThreadLoop();
        return *this;
    }

    HttpListenerBuilder& WithAddr(bool ipV6, std::string ip, size_t port)
    {
        mBuilder.WithAddr(ipV6, std::move(ip), port);
        return *this;
    }

    HttpListenerBuilder& WithReusePort()
    {
        mBuilder.WithReusePort();
        return *this;
    }

    void asyncRun()
    {
        if (mHttpEnterCallback == nullptr)
        {
            throw BrynetCommonException("not setting http enter callback");
        }

        auto callback = mHttpEnterCallback;
        mBuilder.AddEnterCallback([callback](const TcpConnection::Ptr& session) {
            http::HttpService::setup(session, callback);
        });
        mBuilder.asyncRun();
    }

private:
    http::HttpSession::EnterCallback mHttpEnterCallback;
    ListenerBuilder mBuilder;
};

}}}// namespace brynet::net::wrapper
