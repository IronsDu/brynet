#pragma once

#include <chrono>
#include <string>
#include <vector>

#include <brynet/net/TCPService.h>
#include <brynet/net/Connector.h>

namespace brynet { namespace net {

    TcpSocket::Ptr SyncConnectSocket(std::string ip,
        int port,
        std::chrono::milliseconds timeout,
        brynet::net::AsyncConnector::Ptr asyncConnector = nullptr);

    TcpConnection::Ptr SyncConnectSession(std::string ip,
        int port,
        std::chrono::milliseconds timeout,
        brynet::net::TcpService::Ptr service,
        const std::vector<TcpService::AddSocketOption::AddSocketOptionFunc>& options,
        brynet::net::AsyncConnector::Ptr asyncConnector = nullptr);

} }
