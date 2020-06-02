#pragma once

#include <brynet/net/http/HttpService.hpp>
#include <brynet/net/wrapper/ConnectionBuilder.hpp>

namespace brynet { namespace net { namespace wrapper {

    class HttpConnectionBuilder : public BaseConnectionBuilder<HttpConnectionBuilder>
    {
    public:
        HttpConnectionBuilder& configureEnterCallback(
            http::HttpSession::EnterCallback&& callback)
        {
            mHttpEnterCallback = std::move(callback);
            return *this;
        }

        void    asyncConnect() const
        {
            if (mHttpEnterCallback == nullptr)
            {
                throw BrynetCommonException("not setting http enter callback");
            }

            auto connectionOptions = 
                BaseConnectionBuilder<HttpConnectionBuilder>::getConnectionOptions();
            auto callback = mHttpEnterCallback;

            connectionOptions.push_back(
                AddSocketOption::AddEnterCallback(
                    [callback](const TcpConnection::Ptr& session) {
                    http::HttpService::setup(session, callback);
                    }));

            BaseConnectionBuilder<HttpConnectionBuilder>::asyncConnect(
                BaseConnectionBuilder<HttpConnectionBuilder>::getConnectOptions(), 
                connectionOptions);
        }

    private:
        http::HttpSession::EnterCallback    mHttpEnterCallback;
    };

} } }