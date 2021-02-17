#pragma once

#include <brynet/net/SSLHelper.hpp>
#include <brynet/net/TcpConnection.hpp>

namespace brynet { namespace net { namespace detail {

class ConnectionOption final
{
public:
    std::vector<TcpConnection::EnterCallback> enterCallback;
    SSLHelper::Ptr sslHelper;
    bool useSSL = false;
    bool forceSameThreadLoop = false;
    size_t maxRecvBufferSize = 128;
};

}}}// namespace brynet::net::detail
