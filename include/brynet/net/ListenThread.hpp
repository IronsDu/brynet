#pragma once

#include <brynet/net/detail/ListenThreadDetail.hpp>

namespace brynet { namespace net {

class ListenThread : public detail::ListenThreadDetail,
                     public std::enable_shared_from_this<ListenThread>
{
public:
    using Ptr = std::shared_ptr<ListenThread>;
    using AccepCallback = std::function<void(TcpSocket::Ptr)>;
    using TcpSocketProcessCallback = std::function<void(TcpSocket&)>;

    void startListen()
    {
        detail::ListenThreadDetail::startListen();
    }

    void stopListen()
    {
        detail::ListenThreadDetail::stopListen();
    }

public:
    static Ptr Create(bool isIPV6,
                      const std::string& ip,
                      int port,
                      const AccepCallback& callback,
                      const std::vector<TcpSocketProcessCallback>& processCallbacks = {},
                      bool enabledReusePort = false)
    {
        class make_shared_enabler : public ListenThread
        {
        public:
            make_shared_enabler(bool isIPV6,
                                const std::string& ip,
                                int port,
                                const AccepCallback& callback,
                                const std::vector<TcpSocketProcessCallback>& processCallbacks,
                                bool enabledReusePort)
                : ListenThread(isIPV6, ip, port, callback, processCallbacks, enabledReusePort)
            {}
        };
        return std::make_shared<make_shared_enabler>(isIPV6, ip, port, callback, processCallbacks, enabledReusePort);
    }

protected:
    ListenThread(bool isIPV6,
                 const std::string& ip,
                 int port,
                 const AccepCallback& callback,
                 const std::vector<TcpSocketProcessCallback>& processCallbacks,
                 bool enabledReusePort)
        : detail::ListenThreadDetail(isIPV6, ip, port, callback, processCallbacks, enabledReusePort)
    {}
};

}}// namespace brynet::net
