#pragma once

#include <vector>
#include <string>
#include <functional>
#include <thread>
#include <cstdint>
#include <memory>

#include <brynet/net/TcpConnection.h>
#include <brynet/utils/NonCopyable.h>
#include <brynet/utils/Typeids.h>
#include <brynet/net/SSLHelper.h>
#include <brynet/net/Noexcept.h>
#include <brynet/net/Socket.h>

namespace brynet { namespace net {

    class EventLoop;
    class IOLoopData;
    using IOLoopDataPtr = std::shared_ptr<IOLoopData>;

    class TcpService : public utils::NonCopyable, public std::enable_shared_from_this<TcpService>
    {
    public:
        using Ptr = std::shared_ptr<TcpService>;

        using FrameCallback = std::function<void(const EventLoop::Ptr&)>;
        using EnterCallback = std::function<void(const TcpConnection::Ptr&)>;

        class AddSocketOption
        {
        public:
            struct Options;

            using AddSocketOptionFunc = std::function<void(Options& option)>;

            static AddSocketOptionFunc WithEnterCallback(TcpService::EnterCallback callback);
            static AddSocketOptionFunc WithClientSideSSL();
            static AddSocketOptionFunc WithServerSideSSL(SSLHelper::Ptr sslHelper);
            static AddSocketOptionFunc WithMaxRecvBufferSize(size_t size);
            static AddSocketOptionFunc WithForceSameThreadLoop(bool same);
        };

    public:
        static  Ptr                         Create();

        void                                startWorkerThread(size_t threadNum, FrameCallback callback = nullptr);
        void                                stopWorkerThread();
        template<class... Options>
        bool                                addTcpConnection(TcpSocket::Ptr socket, const Options& ... options)
        {
            return _addTcpConnection(std::move(socket), { options... });
        }

        EventLoop::Ptr                      getRandomEventLoop();

    protected:
        TcpService() BRYNET_NOEXCEPT;
        virtual ~TcpService() BRYNET_NOEXCEPT;

        bool                                _addTcpConnection(TcpSocket::Ptr socket,
            const std::vector<AddSocketOption::AddSocketOptionFunc>&);
        EventLoop::Ptr                      getSameThreadEventLoop();

    private:
        std::vector<IOLoopDataPtr>          mIOLoopDatas;
        mutable std::mutex                  mIOLoopGuard;
        std::shared_ptr<bool>               mRunIOLoop;

        std::mutex                          mServiceGuard;
    };

} }