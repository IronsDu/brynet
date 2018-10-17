#pragma once

#include <vector>
#include <string>
#include <functional>
#include <thread>
#include <cstdint>
#include <memory>

#include <brynet/net/DataSocket.h>
#include <brynet/utils/NonCopyable.h>
#include <brynet/utils/Typeids.h>
#include <brynet/net/SSLHelper.h>
#include <brynet/net/Noexcept.h>
#include <brynet/net/Socket.h>

namespace brynet
{
    namespace net
    {
        class EventLoop;
        class IOLoopData;
        typedef std::shared_ptr<IOLoopData> IOLoopDataPtr;

        class TcpService : public NonCopyable, public std::enable_shared_from_this<TcpService>
        {
        public:
            typedef std::shared_ptr<TcpService>                                         PTR;

            typedef std::function<void(const EventLoop::PTR&)>                          FRAME_CALLBACK;
            typedef std::function<void(const DataSocket::PTR&)>                         ENTER_CALLBACK;

            class AddSocketOption
            {
            public:
                struct Options;

                typedef std::function<void(Options& option)> AddSocketOptionFunc;

                static AddSocketOptionFunc WithEnterCallback(TcpService::ENTER_CALLBACK callback);
                static AddSocketOptionFunc WithClientSideSSL();
                static AddSocketOptionFunc WithServerSideSSL(SSLHelper::PTR sslHelper);
                static AddSocketOptionFunc WithMaxRecvBufferSize(size_t size);
                static AddSocketOptionFunc WithForceSameThreadLoop(bool same);
            };

        public:
            static  PTR                         Create();

            void                                startWorkerThread(size_t threadNum, FRAME_CALLBACK callback = nullptr);
            void                                stopWorkerThread();
            template<class... Options>
            bool                                addDataSocket(TcpSocket::PTR socket, const Options& ... options)
            {
                return _addDataSocket(std::move(socket), { options... });
            }

            EventLoop::PTR                      getRandomEventLoop();

        protected:
            TcpService() BRYNET_NOEXCEPT;
            virtual ~TcpService() BRYNET_NOEXCEPT;

            bool                                _addDataSocket(TcpSocket::PTR socket,
                const std::vector<AddSocketOption::AddSocketOptionFunc>&);
            EventLoop::PTR                      getSameThreadEventLoop();

        private:
            std::vector<IOLoopDataPtr>          mIOLoopDatas;
            mutable std::mutex                  mIOLoopGuard;
            std::shared_ptr<bool>               mRunIOLoop;

            std::mutex                          mServiceGuard;
        };
    }
}
