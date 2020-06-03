#pragma once

#include <brynet/net/http/HttpService.hpp>
#include <brynet/net/wrapper/ServiceBuilder.hpp>

namespace brynet { namespace net { namespace wrapper {

    class HttpListenerBuilder : public BaseListenerBuilder<HttpListenerBuilder>
    {
    public:
        HttpListenerBuilder& configureEnterCallback(http::HttpSession::EnterCallback&& callback)
        {
            mHttpEnterCallback = std::move(callback);
            return *this;
        }

        void asyncRun()
        {
            if (mHttpEnterCallback == nullptr)
            {
                throw BrynetCommonException("not setting http enter callback");
            }

            auto connectionOptions = 
                BaseListenerBuilder<HttpListenerBuilder>::getConnectionOptions();
            auto callback = mHttpEnterCallback;
            connectionOptions.push_back(
                AddSocketOption::AddEnterCallback(
                    [callback](const TcpConnection::Ptr& session) {
                    http::HttpService::setup(session, callback);
                }));
            BaseListenerBuilder<HttpListenerBuilder>::asyncRun(connectionOptions);
        }

    private:
        http::HttpSession::EnterCallback    mHttpEnterCallback;
    };

} } }