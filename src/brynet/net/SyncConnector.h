#pragma once

#include <chrono>
#include <string>
#include <vector>

#include <brynet/net/TCPService.h>
#include <brynet/net/Connector.h>

namespace brynet { namespace net {

    TcpSocket::Ptr SyncConnectSocket(std::vector<AsyncConnector::ConnectOptions::ConnectOptionFunc> connectOptions,
        brynet::net::AsyncConnector::Ptr asyncConnector = nullptr);

    TcpConnection::Ptr SyncConnectSession(brynet::net::TcpService::Ptr service,
        std::vector<AsyncConnector::ConnectOptions::ConnectOptionFunc> connectOptions,
        std::vector<TcpService::AddSocketOption::AddSocketOptionFunc> socketOptions,
        brynet::net::AsyncConnector::Ptr asyncConnector = nullptr);

} }
