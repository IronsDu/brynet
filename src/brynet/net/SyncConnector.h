#pragma once

#include <chrono>
#include <string>
#include <vector>

#include <brynet/net/TCPService.h>
#include <brynet/net/Connector.h>

namespace brynet { namespace net {

    TcpSocket::PTR SyncConnectSocket(std::string ip,
        int port,
        std::chrono::milliseconds timeout,
        brynet::net::AsyncConnector::PTR asyncConnector = nullptr);

    DataSocket::PTR SyncConnectSession(std::string ip,
        int port,
        std::chrono::milliseconds timeout,
        brynet::net::TcpService::PTR service,
        const std::vector<TcpService::AddSocketOption::AddSocketOptionFunc>& options,
        brynet::net::AsyncConnector::PTR asyncConnector = nullptr);

} }
