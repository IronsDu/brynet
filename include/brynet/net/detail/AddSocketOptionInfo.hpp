#pragma once

#include <brynet/net/SSLHelper.hpp>
#include <brynet/net/TcpConnection.hpp>

namespace brynet { namespace net { namespace detail {

class AddSocketOptionInfo final
{
public:
    AddSocketOptionInfo()
    {
        useSSL = false;
        forceSameThreadLoop = false;
        maxRecvBufferSize = 128;
    }

    std::vector<TcpConnection::EnterCallback> enterCallback;
    SSLHelper::Ptr sslHelper;
    bool useSSL;
    bool forceSameThreadLoop;
    size_t maxRecvBufferSize;
};

using AddSocketOptionFunc = std::function<void(AddSocketOptionInfo& option)>;

}}}// namespace brynet::net::detail
