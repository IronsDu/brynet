#pragma once

#include <brynet/base/Noexcept.hpp>
#include <brynet/base/NonCopyable.hpp>
#include <brynet/net/AsyncConnector.hpp>
#include <brynet/net/Socket.hpp>
#include <brynet/net/SocketLibFunction.hpp>
#include <brynet/net/wrapper/ConnectionBuilder.hpp>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace brynet { namespace net { namespace detail {

class ListenThreadDetail : public brynet::base::NonCopyable
{
protected:
    using AccepCallback = std::function<void(TcpSocket::Ptr)>;
    using TcpSocketProcessCallback = std::function<void(TcpSocket&)>;

    void startListen()
    {
        std::lock_guard<std::mutex> lck(mListenThreadGuard);

        if (mListenThread != nullptr)
        {
            throw std::runtime_error("listen thread already started");
        }

        const auto fd = brynet::net::base::Listen(mIsIPV6, mIP.c_str(), mPort, 512, mEnabledReusePort);
        if (fd == BRYNET_INVALID_SOCKET)
        {
            throw BrynetCommonException(
                    std::string("listen error of:") + std::to_string(BRYNET_ERRNO));
        }

        mRunListen = std::make_shared<bool>(true);

        auto listenSocket = std::shared_ptr<ListenSocket>(ListenSocket::Create(fd));
        auto isRunListen = mRunListen;
        auto callback = mCallback;
        auto processCallbacks = mProcessCallbacks;
        mListenThread = std::make_shared<std::thread>(
                [isRunListen, listenSocket, callback, processCallbacks]() mutable {
                    while (*isRunListen)
                    {
                        auto clientSocket = runOnceListen(listenSocket);
                        if (clientSocket == nullptr)
                        {
                            continue;
                        }

                        if (*isRunListen)
                        {
                            for (const auto& process : processCallbacks)
                            {
                                process(*clientSocket);
                            }
                            callback(std::move(clientSocket));
                        }
                    }
                });
    }

    void stopListen()
    {
        std::lock_guard<std::mutex> lck(mListenThreadGuard);

        if (mListenThread == nullptr)
        {
            return;
        }

        *mRunListen = false;
        auto selfIP = mIP;
        if (selfIP == "0.0.0.0")
        {
            selfIP = "127.0.0.1";
        }

        auto connector = AsyncConnector::Create();
        connector->startWorkerThread();

        //TODO:: if the listen enable reuse_port, one time connect may be can't wakeup listen.
        wrapper::SocketConnectBuilder connectBuilder;
        (void) connectBuilder
                .WithConnector(connector)
                .WithTimeout(std::chrono::seconds(2))
                .WithAddr(selfIP, mPort)
                .syncConnect();

        try
        {
            if (mListenThread->joinable())
            {
                mListenThread->join();
            }
        }
        catch (std::system_error& e)
        {
            (void) e;
        }
        mListenThread = nullptr;
    }

protected:
    ListenThreadDetail(bool isIPV6,
                       const std::string& ip,
                       int port,
                       const AccepCallback& callback,
                       const std::vector<TcpSocketProcessCallback>& processCallbacks,
                       bool enabledReusePort)
        : mIsIPV6(isIPV6),
          mIP(ip),
          mPort(port),
          mCallback(callback),
          mProcessCallbacks(processCallbacks),
          mEnabledReusePort(enabledReusePort)
    {
        if (mCallback == nullptr)
        {
            throw BrynetCommonException("accept callback is nullptr");
        }
        mRunListen = std::make_shared<bool>(false);
    }

    virtual ~ListenThreadDetail() BRYNET_NOEXCEPT
    {
        stopListen();
    }

private:
    static brynet::net::TcpSocket::Ptr runOnceListen(const std::shared_ptr<ListenSocket>& listenSocket)
    {
        try
        {
            return listenSocket->accept();
        }
        catch (const EintrError& e)
        {
            std::cerr << "accept EINTR execption:" << e.what() << std::endl;
        }
        catch (const AcceptError& e)
        {
            std::cerr << "accept execption:" << e.what() << std::endl;
        }

        return nullptr;
    }

private:
    const bool mIsIPV6;
    const std::string mIP;
    const int mPort;
    const AccepCallback mCallback;
    const std::vector<TcpSocketProcessCallback> mProcessCallbacks;
    const bool mEnabledReusePort;

    std::shared_ptr<bool> mRunListen;
    std::shared_ptr<std::thread> mListenThread;
    std::mutex mListenThreadGuard;
};

}}}// namespace brynet::net::detail
