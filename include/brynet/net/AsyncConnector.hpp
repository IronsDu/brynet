#pragma once

#include <brynet/net/detail/ConnectorDetail.hpp>

namespace brynet { namespace net {

    class ConnectOption final
    {
    public:
        using CompletedCallback = std::function<void(TcpSocket::Ptr)>;
        using ProcessTcpSocketCallback = std::function<void(TcpSocket&)>;
        using FailedCallback = std::function<void()>;
        using ConnectOptionFunc = detail::ConnectOptionFunc;

        static ConnectOptionFunc WithAddr(const std::string& ip, int port)
        {
            return [ip, port](detail::ConnectOptionsInfo& option) {
                option.ip = ip;
                option.port = port;
            };
        }
        static ConnectOptionFunc WithTimeout(std::chrono::nanoseconds timeout)
        {
            return [timeout](detail::ConnectOptionsInfo& option) {
                option.timeout = timeout;
            };
        }
        static ConnectOptionFunc WithCompletedCallback(CompletedCallback callback)
        {
            return [callback](detail::ConnectOptionsInfo& option) {
                option.completedCallback = callback;
            };
        }
        static ConnectOptionFunc AddProcessTcpSocketCallback(ProcessTcpSocketCallback process)
        {
            return [process](detail::ConnectOptionsInfo& option) {
                option.processCallbacks.push_back(process);
            };
        }
        static ConnectOptionFunc WithFailedCallback(FailedCallback callback)
        {
            return [callback](detail::ConnectOptionsInfo& option) {
                option.faledCallback = callback;
            };
        }

        static std::chrono::nanoseconds ExtractTimeout(const std::vector<ConnectOptionFunc>& options)
        {
            detail::ConnectOptionsInfo option;
            for (const auto& func : options)
            {
                func(option);
            }
            return option.timeout;
        }
    };

    class AsyncConnector :  public detail::AsyncConnectorDetail, 
                            public std::enable_shared_from_this<AsyncConnector>
    {
    public:
        using Ptr = std::shared_ptr<AsyncConnector>;

        void    startWorkerThread()
        {
            detail::AsyncConnectorDetail::startWorkerThread();
        }

        void    stopWorkerThread()
        {
            detail::AsyncConnectorDetail::stopWorkerThread();
        }

        void    asyncConnect(const std::vector<detail::ConnectOptionFunc>& options)
        {
            detail::AsyncConnectorDetail::asyncConnect(options);
        }

        static  Ptr Create()
        {
            class make_shared_enabler : public AsyncConnector {};
            return std::make_shared<make_shared_enabler>();
        }

    private:
        AsyncConnector() = default;
    };

} }