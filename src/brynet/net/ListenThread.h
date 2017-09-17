#ifndef BRYNET_NET_TCP_ACCEPTOR_H_
#define BRYNET_NET_TCP_ACCEPTOR_H_

#include <string>
#include <functional>
#include <thread>
#include <memory>
#include <mutex>

#include <brynet/utils/NonCopyable.h>
#include <brynet/utils/Typeids.h>

namespace brynet
{
    namespace net
    {
        class ListenThread : public NonCopyable, public std::enable_shared_from_this<ListenThread>
        {
        public:
            typedef std::shared_ptr<ListenThread>   PTR;
            typedef std::function<void(sock fd)> ACCEPT_CALLBACK;

            /*  开启监听线程  */
            void                                startListen(bool isIPV6, 
                                                            const std::string& ip,
                                                            int port,
                                                            ACCEPT_CALLBACK callback);
            void                                closeListenThread();

            //TODO::将SSL换一个地方
#ifdef USE_OPENSSL
            bool                                initSSL(const std::string& certificate, 
                                                        const std::string& privatekey);
            void                                destroySSL();
            SSL_CTX*                            getOpenSSLCTX();
#endif
            static  PTR                         Create();

        private:
            ListenThread() noexcept;
            virtual ~ListenThread() noexcept;

            void                                runListen();

        private:
            ACCEPT_CALLBACK                     mAcceptCallback;
            bool                                mIsIPV6;
            std::string                         mIP;
            int                                 mPort;
            bool                                mRunListen;
            std::shared_ptr<std::thread>        mListenThread;
            std::mutex                          mListenThreadGuard;
#ifdef USE_OPENSSL
            SSL_CTX*                            mOpenSSLCTX;
#endif
        };
    }
}

#endif
