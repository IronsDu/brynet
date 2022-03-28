#pragma once

#include <brynet/net/detail/TCPServiceDetail.hpp>

namespace brynet { namespace net {

using ConnectionOption = detail::ConnectionOption;

class ITcpService
{
public:
    using Ptr = std::shared_ptr<ITcpService>;

public:
    virtual ~ITcpService() = default;
    virtual bool addTcpConnection(TcpSocket::Ptr socket, ConnectionOption options) = 0;
};

// use multi IO threads process IO
class IOThreadTcpService : public ITcpService,
                           public detail::TcpServiceDetail,
                           public std::enable_shared_from_this<IOThreadTcpService>
{
public:
    using Ptr = std::shared_ptr<IOThreadTcpService>;
    using FrameCallback = detail::TcpServiceDetail::FrameCallback;

public:
    static Ptr Create()
    {
        struct make_shared_enabler : public IOThreadTcpService
        {
        };
        return std::make_shared<make_shared_enabler>();
    }

    std::vector<brynet::net::EventLoop::Ptr> startWorkerThread(size_t threadNum,
                                                               FrameCallback callback = nullptr)
    {
        return detail::TcpServiceDetail::startWorkerThread(threadNum, callback);
    }

    void stopWorkerThread()
    {
        detail::TcpServiceDetail::stopWorkerThread();
    }

    bool addTcpConnection(TcpSocket::Ptr socket, ConnectionOption options) override
    {
        return detail::TcpServiceDetail::addTcpConnection(std::move(socket), options);
    }

private:
    IOThreadTcpService() = default;
};

// use specified eventloop for process IO
class EventLoopTcpService : public ITcpService
{
public:
    using Ptr = std::shared_ptr<EventLoopTcpService>;

    EventLoopTcpService(EventLoop::Ptr eventLoop)
        : mEventLoop(eventLoop)
    {}

    static Ptr Create(EventLoop::Ptr eventLoop)
    {
        return std::make_shared<EventLoopTcpService>(eventLoop);
    }

    bool addTcpConnection(TcpSocket::Ptr socket, ConnectionOption options) override
    {
        return detail::HelperAddTcpConnection(mEventLoop, std::move(socket), options);
    }

private:
    EventLoop::Ptr mEventLoop;
};

}}// namespace brynet::net
