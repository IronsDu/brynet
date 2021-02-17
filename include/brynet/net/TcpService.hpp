#pragma once

#include <brynet/net/detail/TCPServiceDetail.hpp>

namespace brynet { namespace net {

using ConnectionOption = detail::ConnectionOption;
class TcpService : public detail::TcpServiceDetail,
                   public std::enable_shared_from_this<TcpService>
{
public:
    using Ptr = std::shared_ptr<TcpService>;
    using FrameCallback = detail::TcpServiceDetail::FrameCallback;

public:
    static Ptr Create()
    {
        struct make_shared_enabler : public TcpService
        {
        };
        return std::make_shared<make_shared_enabler>();
    }

    void startWorkerThread(size_t threadNum,
                           FrameCallback callback = nullptr)
    {
        detail::TcpServiceDetail::startWorkerThread(threadNum, callback);
    }

    void stopWorkerThread()
    {
        detail::TcpServiceDetail::stopWorkerThread();
    }

    bool addTcpConnection(TcpSocket::Ptr socket, ConnectionOption options)
    {
        return detail::TcpServiceDetail::addTcpConnection(std::move(socket), options);
    }

    EventLoop::Ptr getRandomEventLoop()
    {
        return detail::TcpServiceDetail::getRandomEventLoop();
    }

private:
    TcpService() = default;
};

}}// namespace brynet::net
