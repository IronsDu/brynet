#pragma once

#include <brynet/net/Exception.hpp>
#include <brynet/net/ListenThread.hpp>
#include <brynet/net/TcpService.hpp>
#include <brynet/net/detail/ConnectionOption.hpp>
#include <utility>

namespace brynet { namespace net { namespace wrapper {

template<typename Derived>
class BaseListenerBuilder
{
public:
    virtual ~BaseListenerBuilder() = default;

    Derived& WithService(TcpService::Ptr service)
    {
        mTcpService = std::move(service);
        return static_cast<Derived&>(*this);
    }

    Derived& WithAddr(bool ipV6, std::string ip, size_t port)
    {
        mIsIpV6 = ipV6;
        mListenAddr = std::move(ip);
        mPort = port;
        return static_cast<Derived&>(*this);
    }

    Derived& WithReusePort()
    {
        mEnabledReusePort = true;
        return static_cast<Derived&>(*this);
    }

    Derived& AddSocketProcess(const ListenThread::TcpSocketProcessCallback& callback)
    {
        mSocketProcessCallbacks.push_back(callback);
        return static_cast<Derived&>(*this);
    }

    Derived& WithMaxRecvBufferSize(size_t size)
    {
        mSocketOption.maxRecvBufferSize = size;
        return static_cast<Derived&>(*this);
    }
#ifdef BRYNET_USE_OPENSSL
    Derived& WithSSL(SSLHelper::Ptr sslHelper)
    {
        mSocketOption.sslHelper = std::move(sslHelper);
        mSocketOption.useSSL = true;
        return static_cast<Derived&>(*this);
    }
#endif
    Derived& WithForceSameThreadLoop()
    {
        mSocketOption.forceSameThreadLoop = true;
        return static_cast<Derived&>(*this);
    }

    Derived& AddEnterCallback(const TcpConnection::EnterCallback& callback)
    {
        mSocketOption.enterCallback.push_back(callback);
        return static_cast<Derived&>(*this);
    }

    void asyncRun()
    {
        if (mTcpService == nullptr)
        {
            throw BrynetCommonException("tcp service is nullptr");
        }
        if (mListenAddr.empty())
        {
            throw BrynetCommonException("not config listen addr");
        }

        auto service = mTcpService;
        auto option = mSocketOption;
        mListenThread = ListenThread::Create(
                mIsIpV6,
                mListenAddr,
                mPort,
                [service, option](brynet::net::TcpSocket::Ptr socket) {
                    service->addTcpConnection(std::move(socket), option);
                },
                mSocketProcessCallbacks,
                mEnabledReusePort);
        mListenThread->startListen();
    }

    void stop()
    {
        if (mListenThread)
        {
            mListenThread->stopListen();
        }
    }

private:
    TcpService::Ptr mTcpService;
    std::vector<ListenThread::TcpSocketProcessCallback> mSocketProcessCallbacks;
    ConnectionOption mSocketOption;
    std::string mListenAddr;
    int mPort = 0;
    bool mIsIpV6 = false;
    bool mEnabledReusePort = false;
    ListenThread::Ptr mListenThread;
};

class ListenerBuilder : public BaseListenerBuilder<ListenerBuilder>
{
};

}}}// namespace brynet::net::wrapper
