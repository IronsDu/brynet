#pragma once

#include <memory>
#include <functional>
#include <deque>
#include <chrono>

#include <brynet/net/Channel.h>
#include <brynet/net/EventLoop.h>
#include <brynet/timer/Timer.h>
#include <brynet/utils/NonCopyable.h>
#include <brynet/net/Any.h>
#include <brynet/net/Noexcept.h>
#include <brynet/net/Socket.h>
#include <brynet/utils/buffer.h>

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

namespace brynet { namespace net {

    class TcpConnection : public Channel, public utils::NonCopyable, public std::enable_shared_from_this<TcpConnection>
    {
    public:
        using Ptr = std::shared_ptr<TcpConnection>;

        using EnterCallback = std::function<void(Ptr)>;
        using DataCallback = std::function<size_t(const char* buffer, size_t len)>;
        using DisconnectedCallback = std::function<void(Ptr)>;
        using PacketSendedCallback = std::function<void(void)>;

        using PacketPtr = std::shared_ptr<std::string>;

    public:
        Ptr static Create(TcpSocket::Ptr, size_t maxRecvBufferSize, EnterCallback&&, EventLoop::Ptr);

        /* must called in network thread */
        bool                            onEnterEventLoop();
        const EventLoop::Ptr&           getEventLoop() const;

        //TODO::如果所属EventLoop已经没有工作，则可能导致内存无限大，因为所投递的请求都没有得到处理

        void                            send(const char* buffer, size_t len, PacketSendedCallback&& callback = nullptr);
        template<typename PacketType>
        void                            send(PacketType&& packet, PacketSendedCallback&& callback = nullptr)
        {
            auto packetCapture = std::forward<PacketType>(packet);
            auto callbackCapture = std::forward<PacketSendedCallback>(callback);
            auto sharedThis = shared_from_this();
            mEventLoop->runAsyncFunctor([sharedThis, packetCapture, callbackCapture]() mutable {
                    const auto len = packetCapture->size();
                    sharedThis->mSendList.emplace_back(PendingPacket{ std::move(packetCapture), len, std::move(callbackCapture) });
                    sharedThis->runAfterFlush();
                });
        }

        void                            setDataCallback(DataCallback&& cb);
        void                            setDisConnectCallback(DisconnectedCallback&& cb);

        /* if checkTime is zero, will cancel check heartbeat */
        void                            setHeartBeat(std::chrono::nanoseconds checkTime);
        void                            postDisConnect();
        void                            postShutdown();

        void                            setUD(BrynetAny value);
        const BrynetAny&                getUD() const;

        const std::string&              getIP() const;

#ifdef USE_OPENSSL
        bool                            initAcceptSSL(SSL_CTX*);
        bool                            initConnectSSL();
#endif

        static  TcpConnection::PacketPtr  makePacket(const char* buffer, size_t len);

    protected:
        TcpConnection(TcpSocket::Ptr, size_t maxRecvBufferSize, EnterCallback&&, EventLoop::Ptr) BRYNET_NOEXCEPT;
        virtual ~TcpConnection() BRYNET_NOEXCEPT;

    private:
        void                            growRecvBuffer();

        void                            pingCheck();
        void                            startPingCheckTimer();

        void                            canRecv() override;
        void                            canSend() override;

        bool                            checkRead();
        bool                            checkWrite();

        void                            recv();
        void                            flush();
        void                            normalFlush();
#if defined PLATFORM_LINUX || defined PLATFORM_DARWIN
        void                            quickFlush();
#endif

        void                            onClose() override;
        void                            procCloseInLoop();
        void                            procShutdownInLoop();

        void                            runAfterFlush();
#if defined PLATFORM_LINUX || defined PLATFORM_DARWIN
        void                            removeCheckWrite();
#endif
#ifdef USE_OPENSSL
        bool                            processSSLHandshake();
#endif
        void                            causeEnterCallback();

    private:

#ifdef PLATFORM_WINDOWS
        struct port::Win::OverlappedExt mOvlRecv;
        struct port::Win::OverlappedExt mOvlSend;

        bool                            mPostRecvCheck;
        bool                            mPostWriteCheck;
        bool                            mPostClose;
#endif
        const std::string               mIP;
        const TcpSocket::Ptr            mSocket;
        const EventLoop::Ptr            mEventLoop;
        bool                            mCanWrite;
        bool                            mAlreadyClose;

        class BufferDeleter
        {
        public:
            void operator()(struct buffer_s* ptr) const
            {
                ox_buffer_delete(ptr);
            }
        };
        std::unique_ptr<struct buffer_s, BufferDeleter> mRecvBuffer;
        const size_t                    mMaxRecvBufferSize;

        struct PendingPacket
        {
            PacketPtr  data;
            size_t      left;
            PacketSendedCallback  mCompleteCallback;
        };

        using PacketListType = std::deque<PendingPacket>;
        PacketListType                  mSendList;

        EnterCallback                   mEnterCallback;
        DataCallback                    mDataCallback;
        DisconnectedCallback            mDisConnectCallback;

        bool                            mIsPostFlush;

        BrynetAny                       mUD;

#ifdef USE_OPENSSL
        SSL_CTX*                        mSSLCtx;
        SSL*                            mSSL;
        bool                            mIsHandsharked;
#endif
        bool                            mRecvData;
        std::chrono::nanoseconds        mCheckTime;
        timer::Timer::WeakPtr           mTimer;
    };

} }