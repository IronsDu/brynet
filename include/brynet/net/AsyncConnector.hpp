#pragma once

#include <brynet/net/detail/ConnectorDetail.hpp>

namespace brynet { namespace net {

class AsyncConnector : public detail::AsyncConnectorDetail,
                       public std::enable_shared_from_this<AsyncConnector>
{
public:
    using Ptr = std::shared_ptr<AsyncConnector>;

    void startWorkerThread()
    {
        detail::AsyncConnectorDetail::startWorkerThread();
    }

    void stopWorkerThread()
    {
        detail::AsyncConnectorDetail::stopWorkerThread();
    }

    void asyncConnect(const std::vector<detail::ConnectOptionFunc>& options)
    {
        detail::AsyncConnectorDetail::asyncConnect(options);
    }

    void asyncConnect(const detail::ConnectOptionsInfo& info)
    {
        detail::AsyncConnectorDetail::asyncConnect(info);
    }

    static Ptr Create()
    {
        class make_shared_enabler : public AsyncConnector
        {
        };
        return std::make_shared<make_shared_enabler>();
    }

private:
    AsyncConnector() = default;
};

}}// namespace brynet::net
