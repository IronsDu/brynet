#ifndef BRYNET_NET_SYNC_CONNECT_H_
#define BRYNET_NET_SYNC_CONNECT_H_

#include <chrono>
#include <string>
#include <vector>

#include <brynet/net/WrapTCPService.h>
#include <brynet/net/Connector.h>

namespace brynet
{
    namespace net
    {
        TcpSocket::PTR SyncConnectSocket(std::string ip,
            int port,
            std::chrono::milliseconds timeout,
            brynet::net::AsyncConnector::PTR asyncConnector = nullptr);

        TCPSession::PTR SyncConnectSession(std::string ip,
            int port,
            std::chrono::milliseconds timeout,
            brynet::net::WrapTcpService::PTR service,
            const std::vector<AddSessionOption::AddSessionOptionFunc>& options,
            brynet::net::AsyncConnector::PTR asyncConnector = nullptr);
    }
}

#endif
