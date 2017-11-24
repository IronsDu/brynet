#ifndef BRYNET_NET_TCP_ACCEPTOR_H_
#define BRYNET_NET_TCP_ACCEPTOR_H_

#include <string>
#include <functional>
#include <thread>
#include <memory>
#include <mutex>

#include <brynet/utils/NonCopyable.h>
#include <brynet/utils/Typeids.h>
#include <brynet/net/SocketLibFunction.h>
#include <brynet/net/Noexcept.h>

namespace brynet
{
    namespace net
    {
        class ListenThread : public NonCopyable, public std::enable_shared_from_this<ListenThread>
        {
        public:
            typedef std::shared_ptr<ListenThread>   PTR;
            typedef std::function<void(sock fd)> ACCEPT_CALLBACK;

            void                                startListen(bool isIPV6, 
                                                            const std::string& ip,
                                                            int port,
                                                            ACCEPT_CALLBACK callback);
            void                                stopListen();
            static  PTR                         Create();

        private:
            ListenThread() BRYNET_NOEXCEPT;
            virtual ~ListenThread() BRYNET_NOEXCEPT;

            void                                runListen(sock fd);

        private:
            ACCEPT_CALLBACK                     mAcceptCallback;
            bool                                mIsIPV6;
            std::string                         mIP;
            int                                 mPort;
            bool                                mRunListen;
            std::shared_ptr<std::thread>        mListenThread;
            std::mutex                          mListenThreadGuard;
        };
    }
}

#endif
