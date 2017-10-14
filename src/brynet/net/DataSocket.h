#ifndef BRYNET_NET_DATASOCKET_H_
#define BRYNET_NET_DATASOCKET_H_

#include <memory>
#include <functional>
#include <deque>
#include <chrono>

#include <brynet/net/Channel.h>
#include <brynet/net/EventLoop.h>
#include <brynet/timer/Timer.h>
#include <brynet/utils/NonCopyable.h>
#include <brynet/net/Any.h>

#ifdef USE_OPENSSL

#ifdef  __cplusplus
extern "C" {
#endif
#include <openssl/ssl.h>
#include <openssl/err.h>
#ifdef  __cplusplus
}
#endif

#endif

struct buffer_s;

namespace brynet
{
    namespace net
    {
        class DataSocket final : public Channel, public NonCopyable
        {
        public:
            typedef DataSocket*                                                         PTR;

            typedef std::function<void(PTR)>                                            ENTER_CALLBACK;
            typedef std::function<size_t(PTR, const char* buffer, size_t len)>          DATA_CALLBACK;
            typedef std::function<void(PTR)>                                            DISCONNECT_CALLBACK;
            typedef std::shared_ptr<std::function<void(void)>>                          PACKED_SENDED_CALLBACK;

            typedef std::shared_ptr<std::string>                                        PACKET_PTR;

        public:
            DataSocket(sock fd, size_t maxRecvBufferSize) noexcept;
            ~DataSocket() noexcept;

            /* must called in network thread */
            bool                            onEnterEventLoop(const EventLoop::PTR& eventLoop);

            void                            send(const char* buffer, size_t len, const PACKED_SENDED_CALLBACK& callback = nullptr);
            void                            sendPacket(const PACKET_PTR&, const PACKED_SENDED_CALLBACK& callback = nullptr);
            void                            sendPacketInLoop(const PACKET_PTR&, const PACKED_SENDED_CALLBACK& callback = nullptr);

            void                            setEnterCallback(ENTER_CALLBACK cb);
            void                            setDataCallback(DATA_CALLBACK cb);
            void                            setDisConnectCallback(DISCONNECT_CALLBACK cb);

            /* if checkTime is zero, will cancel check heartbeat */
            void                            setCheckTime(std::chrono::nanoseconds checkTime);
            void                            postDisConnect();
            void                            postShutdown();

            void                            setUD(BrynetAny value);
            const BrynetAny&                getUD() const;

#ifdef USE_OPENSSL
            bool                            initAcceptSSL(SSL_CTX*);
            bool                            initConnectSSL();
#endif

            static  DataSocket::PACKET_PTR  makePacket(const char* buffer, size_t len);

        private:
            void                            growRecvBuffer();

            void                            PingCheck();
            void                            startPingCheckTimer();

            void                            canRecv() override;
            void                            canSend() override;

            bool                            checkRead();
            bool                            checkWrite();

            void                            recv();
            void                            flush();
            void                            normalFlush();
            void                            quickFlush();

            void                            onClose() override;
            void                            closeSocket();
            void                            procCloseInLoop();
            void                            procShutdownInLoop();

            void                            runAfterFlush();
#ifdef PLATFORM_LINUX
            void                            removeCheckWrite();
#endif
#ifdef USE_OPENSSL
            void                            processSSLHandshake();
#endif

        private:

#ifdef PLATFORM_WINDOWS
            struct EventLoop::ovl_ext_s     mOvlRecv;
            struct EventLoop::ovl_ext_s     mOvlSend;

            bool                            mPostRecvCheck;
            bool                            mPostWriteCheck;
#endif

            sock                            mFD;
            bool                            mIsPostFinalClose;

            bool                            mCanWrite;

            EventLoop::PTR                  mEventLoop;
            buffer_s*                       mRecvBuffer;
            size_t                          mMaxRecvBufferSize;

            struct pending_packet
            {
                PACKET_PTR  data;
                size_t      left;
                PACKED_SENDED_CALLBACK  mCompleteCallback;
            };

            typedef std::deque<pending_packet>   PACKET_LIST_TYPE;
            PACKET_LIST_TYPE                mSendList;

            ENTER_CALLBACK                  mEnterCallback;
            DATA_CALLBACK                   mDataCallback;
            DISCONNECT_CALLBACK             mDisConnectCallback;

            bool                            mIsPostFlush;

            BrynetAny                       mUD;

#ifdef USE_OPENSSL
            SSL_CTX*                        mSSLCtx;
            SSL*                            mSSL;
            bool                            mIsHandsharked;
#endif

            bool                            mRecvData;
            std::chrono::nanoseconds        mCheckTime;
            Timer::WeakPtr                  mTimer;
        };
    }
}

#endif