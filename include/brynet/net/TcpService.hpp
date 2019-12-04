#pragma once

#include <brynet/net/detail/TCPServiceDetail.hpp>

namespace brynet { namespace net {

    class AddSocketOption
    {
    private:
        using AddSocketOptionFunc = detail::AddSocketOptionFunc;
        using AddSocketOptionInfo = detail::AddSocketOptionInfo;

    public:
        static AddSocketOptionFunc AddEnterCallback(
            TcpConnection::EnterCallback callback)
        {
            return [callback](AddSocketOptionInfo& option) {
                option.enterCallback.push_back(callback);
            };
        }
#ifdef BRYNET_USE_OPENSSL
        static AddSocketOptionFunc WithClientSideSSL()
        {
            return [](AddSocketOptionInfo& option) {
                option.useSSL = true;
            };
        }
        static AddSocketOptionFunc WithServerSideSSL(SSLHelper::Ptr sslHelper)
        {
            return [sslHelper](AddSocketOptionInfo& option) {
                option.sslHelper = sslHelper;
                option.useSSL = true;
            };
        }
#endif
        static AddSocketOptionFunc WithMaxRecvBufferSize(size_t size)
        {
            return [size](AddSocketOptionInfo& option) {
                option.maxRecvBufferSize = size;
            };
        }
        static AddSocketOptionFunc WithForceSameThreadLoop(bool same)
        {
            return [same](AddSocketOptionInfo& option) {
                option.forceSameThreadLoop = same;
            };
        }
    };

    class TcpService :  public detail::TcpServiceDetail,
                        public std::enable_shared_from_this<TcpService>
    {
    public:
        using Ptr = std::shared_ptr<TcpService>;
        using FrameCallback = detail::TcpServiceDetail::FrameCallback;

    public:
        static  Ptr Create()
        {
            struct make_shared_enabler : public TcpService {};
            return std::make_shared<make_shared_enabler>();
        }

        void        startWorkerThread(size_t threadNum, 
            FrameCallback callback = nullptr)
        {
            detail::TcpServiceDetail::startWorkerThread(threadNum, callback);
        }

        void        stopWorkerThread()
        {
            detail::TcpServiceDetail::stopWorkerThread();
        }

        template<typename... Options>
        bool            addTcpConnection(TcpSocket::Ptr socket, 
            const Options& ... options)
        {
            return detail::TcpServiceDetail::addTcpConnection(std::move(socket), options...);
        }

        EventLoop::Ptr  getRandomEventLoop()
        {
            return detail::TcpServiceDetail::getRandomEventLoop();
        }

    private:
        TcpService() = default;
    };

} }