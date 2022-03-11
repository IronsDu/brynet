﻿#pragma once

#include <brynet/base/Any.hpp>
#include <brynet/base/Buffer.hpp>
#include <brynet/base/Noexcept.hpp>
#include <brynet/base/NonCopyable.hpp>
#include <brynet/base/Packet.hpp>
#include <brynet/base/Timer.hpp>
#include <brynet/net/Channel.hpp>
#include <brynet/net/EventLoop.hpp>
#include <brynet/net/SSLHelper.hpp>
#include <brynet/net/SendableMsg.hpp>
#include <brynet/net/Socket.hpp>
#include <brynet/net/SocketLibFunction.hpp>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstring>
#include <deque>
#include <functional>
#include <memory>
#include <type_traits>

#ifdef BRYNET_USE_OPENSSL

#ifdef __cplusplus
extern "C" {
#endif
#include <openssl/err.h>
#include <openssl/ssl.h>
#ifdef __cplusplus
}
#endif

#endif

namespace brynet { namespace net {

class TcpConnection : public Channel,
                      public brynet::base::NonCopyable,
                      public std::enable_shared_from_this<TcpConnection>
{
public:
    using Ptr = std::shared_ptr<TcpConnection>;

    using EnterCallback = std::function<void(Ptr)>;
    using DataCallback = std::function<void(brynet::base::BasePacketReader&)>;
    using DisconnectedCallback = std::function<void(Ptr)>;
    using PacketSendedCallback = std::function<void(void)>;
    using HighWaterCallback = std::function<void(void)>;

public:
    Ptr static Create(TcpSocket::Ptr socket,
                      size_t maxRecvBufferSize,
                      EnterCallback&& enterCallback,
                      const EventLoop::Ptr& eventLoop,
                      const SSLHelper::Ptr& sslHelper = nullptr)
    {
        class make_shared_enabler : public TcpConnection
        {
        public:
            make_shared_enabler(TcpSocket::Ptr socket,
                                size_t maxRecvBufferSize,
                                EnterCallback&& enterCallback,
                                EventLoop::Ptr eventLoop)
                : TcpConnection(std::move(socket),
                                maxRecvBufferSize,
                                std::move(enterCallback),
                                std::move(eventLoop))
            {}
        };

        const auto isServerSide = socket->isServerSide();
        auto session = std::make_shared<make_shared_enabler>(
                std::move(socket),
                maxRecvBufferSize,
                std::move(enterCallback),
                eventLoop);
        (void) isServerSide;
#ifdef BRYNET_USE_OPENSSL
        if (sslHelper != nullptr)
        {
            if (isServerSide)
            {
                if (sslHelper->getOpenSSLCTX() == nullptr ||
                    !session->initAcceptSSL(sslHelper->getOpenSSLCTX()))
                {
                    throw std::runtime_error("init ssl failed");
                }
            }
            else
            {
                if (!session->initConnectSSL())
                {
                    throw std::runtime_error("init ssl failed");
                }
            }
        }
#else
        if (sslHelper != nullptr)
        {
            throw std::runtime_error("not enable ssl");
        }
#endif

        eventLoop->runAsyncFunctor([session]() {
            session->onEnterEventLoop();
        });
        return session;
    }

    const EventLoop::Ptr& getEventLoop() const
    {
        return mEventLoop;
    }

    //TODO::如果所属EventLoop已经没有工作，则可能导致内存无限大，因为所投递的请求都没有得到处理
    void send(const SendableMsg::Ptr& msg,
              PacketSendedCallback&& callback = nullptr)
    {
        if (mEventLoop->isInLoopThread())
        {
            sendInLoop(msg, std::move(callback));
        }
        else
        {
            auto sharedThis = shared_from_this();
            mEventLoop->runAsyncFunctor([sharedThis, msg, callback, this]() mutable {
                sendInLoop(msg, std::move(callback));
            });
        }
    }

    void send(const char* buffer,
              size_t len,
              PacketSendedCallback&& callback = nullptr)
    {
        send(MakeStringMsg(buffer, len), std::move(callback));
    }

    void send(const std::string& buffer,
              PacketSendedCallback&& callback = nullptr)
    {
        send(MakeStringMsg(buffer), std::move(callback));
    }

    void send(std::string&& buffer,
              PacketSendedCallback&& callback = nullptr)
    {
        send(MakeStringMsg(std::move(buffer)), std::move(callback));
    }

    // setDataCallback(std::function<void(brynet::base::BasePacketReader&)>）
    template<typename Callback>
    void setDataCallback(Callback&& cb)
    {
        verifyArgType(cb, &Callback::operator());

        auto sharedThis = shared_from_this();
        mEventLoop->runAsyncFunctor([sharedThis, cb, this]() mutable {
            mDataCallback = cb;
            processRecvMessage();
        });
    }

    void setDisConnectCallback(DisconnectedCallback&& cb)
    {
        auto sharedThis = shared_from_this();
        mEventLoop->runAsyncFunctor([sharedThis, cb, this]() mutable {
            mDisConnectCallback = std::move(cb);
        });
    }

    /* if checkTime is zero, will cancel check heartbeat */
    void setHeartBeat(std::chrono::nanoseconds checkTime)
    {
        auto sharedThis = shared_from_this();
        mEventLoop->runAsyncFunctor([sharedThis, checkTime, this]() {
            if (mTimer.lock() != nullptr)
            {
                mTimer.lock()->cancel();
                mTimer.reset();
            }

            mCheckTime = checkTime;
            startPingCheckTimer();
        });
    }

    void setHighWaterCallback(HighWaterCallback cb, size_t size)
    {
        auto sharedThis = shared_from_this();
        mEventLoop->runAsyncFunctor([=]() mutable {
            mHighWaterCallback = std::move(cb);
            mHighWaterSize = size;
        });
    }

    void postShrinkReceiveBuffer()
    {
        auto sharedThis = shared_from_this();
        mEventLoop->runAsyncFunctor([sharedThis, this]() {
            mEventLoop->runFunctorAfterLoop([sharedThis, this]() {
                shrinkReceiveBuffer();
            });
        });
    }

    void postDisConnect()
    {
        auto sharedThis = shared_from_this();
        mEventLoop->runAsyncFunctor([sharedThis, this]() {
            // call runFunctorAfterLoop, avoid user call postDisConnect in message callback, because after it brynet will use receive buffer.
            mEventLoop->runFunctorAfterLoop([sharedThis, this]() {
                procCloseInLoop();
            });
        });
    }

    void postShutdown()
    {
        auto sharedThis = shared_from_this();
        mEventLoop->runAsyncFunctor([sharedThis, this]() {
            mEventLoop->runFunctorAfterLoop([sharedThis, this]() {
                procShutdownInLoop();
            });
        });
    }

    const std::string& getIP() const
    {
        return mIP;
    }

protected:
    TcpConnection(TcpSocket::Ptr socket,
                  size_t maxRecvBufferSize,
                  EnterCallback&& enterCallback,
                  EventLoop::Ptr eventLoop) BRYNET_NOEXCEPT
        :
#ifdef BRYNET_PLATFORM_WINDOWS
        mOvlRecv(port::Win::OverlappedType::OverlappedRecv),
        mOvlSend(port::Win::OverlappedType::OverlappedSend),
        mPostClose(false),
#endif
        mIP(socket->getRemoteIP()),
        mSocket(std::move(socket)),
        mEventLoop(std::move(eventLoop)),
        mAlreadyClose(false),
        mMaxRecvBufferSize(maxRecvBufferSize),
        mSendingMsgSize(0),
        mEnterCallback(std::move(enterCallback)),
        mHighWaterSize(0)
    {
        mRecvData = false;
        mCheckTime = std::chrono::steady_clock::duration::zero();
        mIsPostFlush = false;

        mCanWrite = true;

#ifdef BRYNET_PLATFORM_WINDOWS
        mPostRecvCheck = false;
        mPostWriteCheck = false;
#endif
        growRecvBuffer();

#ifdef BRYNET_USE_OPENSSL
        mSSLCtx = nullptr;
        mSSL = nullptr;
        mIsHandsharked = false;
#endif
    }

    ~TcpConnection() BRYNET_NOEXCEPT override
    {
#ifdef BRYNET_USE_OPENSSL
        if (mSSL != nullptr)
        {
            SSL_free(mSSL);
            mSSL = nullptr;
        }
        if (mSSLCtx != nullptr)
        {
            SSL_CTX_free(mSSLCtx);
            mSSLCtx = nullptr;
        }
#endif

        if (mTimer.lock())
        {
            mTimer.lock()->cancel();
        }
    }

private:
    void sendInLoop(const SendableMsg::Ptr& msg,
                    PacketSendedCallback&& callback = nullptr)
    {
        if (mAlreadyClose)
        {
            return;
        }

        const auto len = msg->size();
        mSendingMsgSize += len;
        mSendList.emplace_back(PendingPacket{
                msg,
                len,
                std::move(callback)});
        runAfterFlush();

        if (mSendingMsgSize > mHighWaterSize &&
            mHighWaterCallback != nullptr)
        {
            mHighWaterCallback();
        }
    }

    void growRecvBuffer()
    {
        if (mRecvBuffer == nullptr)
        {
            mRecvBuffer.reset(brynet::base::buffer_new(std::min<size_t>(16 * 1024, mMaxRecvBufferSize)));
            mRecvBuffOriginSize = buffer_getsize(mRecvBuffer.get());
        }
        else
        {
            if (buffer_getsize(mRecvBuffer.get()) >= mMaxRecvBufferSize)
            {
                return;
            }

            mCurrentTanhXDiff += 0.2;
            const auto newTanh = std::tanh(mCurrentTanhXDiff);
            const auto maxSizeDiff = mMaxRecvBufferSize - mRecvBuffOriginSize;
            const auto NewSize = mRecvBuffOriginSize + (maxSizeDiff * newTanh);

            assert(NewSize <= mMaxRecvBufferSize);
            std::unique_ptr<struct brynet::base::buffer_s, BufferDeleter> newBuffer(brynet::base::buffer_new(NewSize));
            buffer_write(newBuffer.get(),
                         buffer_getreadptr(mRecvBuffer.get()),
                         buffer_getreadvalidcount(mRecvBuffer.get()));
            mRecvBuffer = std::move(newBuffer);
        }
    }

    void shrinkReceiveBuffer()
    {
        auto newSize = buffer_getreadvalidcount(mRecvBuffer.get());
        if (newSize == 0)
        {
            newSize = std::min<size_t>(16 * 1024, mMaxRecvBufferSize);
        }
        if (newSize == buffer_getsize(mRecvBuffer.get()))
        {
            return;
        }

        std::unique_ptr<struct brynet::base::buffer_s, BufferDeleter>
                newBuffer(brynet::base::buffer_new(newSize));
        buffer_write(newBuffer.get(),
                     buffer_getreadptr(mRecvBuffer.get()),
                     buffer_getreadvalidcount(mRecvBuffer.get()));

        mRecvBuffer = std::move(newBuffer);
        mCurrentTanhXDiff = 0;
        mRecvBuffOriginSize = newSize;
    }

    /* must called in network thread */
    bool onEnterEventLoop()
    {
        assert(mEventLoop->isInLoopThread());
        if (!mEventLoop->isInLoopThread())
        {
            return false;
        }

        if (!brynet::net::base::SocketNonblock(mSocket->getFD()) ||
            !mEventLoop->linkChannel(mSocket->getFD(), this))
        {
            return false;
        }

        const auto findRet = mEventLoop->getTcpConnection(mSocket->getFD());
        (void) findRet;
        assert(findRet == nullptr);

#ifdef BRYNET_USE_OPENSSL
        if (mSSL != nullptr)
        {
            mEventLoop->addTcpConnection(mSocket->getFD(), shared_from_this());
            processSSLHandshake();
            return true;
        }
#endif

        if (!checkRead())
        {
            return false;
        }

        mEventLoop->addTcpConnection(mSocket->getFD(), shared_from_this());
        causeEnterCallback();

        return true;
    }

#ifdef BRYNET_USE_OPENSSL
    bool initAcceptSSL(SSL_CTX* ctx)
    {
        if (mSSL != nullptr)
        {
            return false;
        }

        mSSL = SSL_new(ctx);
        if (SSL_set_fd(mSSL, mSocket->getFD()) != 1)
        {
            ERR_print_errors_fp(stdout);
            ::fflush(stdout);
            return false;
        }

        return true;
    }
    bool initConnectSSL()
    {
        if (mSSLCtx != nullptr)
        {
            return false;
        }

        mSSLCtx = SSL_CTX_new(SSLv23_client_method());
        mSSL = SSL_new(mSSLCtx);

        if (SSL_set_fd(mSSL, mSocket->getFD()) != 1)
        {
            ERR_print_errors_fp(stdout);
            ::fflush(stdout);
            return false;
        }

        return true;
    }
#endif

    void pingCheck()
    {
        mTimer.reset();
        if (mRecvData)
        {
            mRecvData = false;
            startPingCheckTimer();
        }
        else
        {
            procCloseInLoop();
        }
    }

    void startPingCheckTimer()
    {
        if (!mTimer.lock() &&
            mCheckTime != std::chrono::steady_clock::duration::zero())
        {
            std::weak_ptr<TcpConnection> weakThis = shared_from_this();
            mTimer = mEventLoop->runAfter(mCheckTime, [weakThis]() {
                auto sharedThis = weakThis.lock();
                if (sharedThis != nullptr)
                {
                    sharedThis->pingCheck();
                }
            });
        }
    }

    void canRecv(const bool willClose) override
    {
#ifdef BRYNET_PLATFORM_WINDOWS
        mPostRecvCheck = false;
        if (mPostClose)
        {
            if (!mPostWriteCheck)
            {
                onClose();
            }
            return;
        }
#elif defined BRYNET_PLATFORM_LINUX || defined BRYNET_PLATFORM_DARWIN
        if (mAlreadyClose)
        {
            return;
        }
#endif

#ifdef BRYNET_USE_OPENSSL
        if (mSSL != nullptr && !mIsHandsharked && (!processSSLHandshake() || !mIsHandsharked))
        {
            return;
        }
#endif

        do
        {
            recv();
            adjustReceiveBuffer();
        } while (willClose && !mAlreadyClose && mRecvBuffer != nullptr && buffer_getwritevalidcount(mRecvBuffer.get()) > 0);
    }

    void canSend() override
    {
#ifdef BRYNET_PLATFORM_WINDOWS
        mPostWriteCheck = false;
        if (mPostClose)
        {
            if (!mPostRecvCheck)
            {
                onClose();
            }
            return;
        }
#elif defined BRYNET_PLATFORM_LINUX || defined BRYNET_PLATFORM_DARWIN
        if (mAlreadyClose)
        {
            return;
        }
#endif
        mCanWrite = true;

#ifdef BRYNET_USE_OPENSSL
        if (mSSL != nullptr && !mIsHandsharked && (!processSSLHandshake() || !mIsHandsharked))
        {
            return;
        }
#endif

        runAfterFlush();
    }

    bool checkRead()
    {
        bool check_ret = true;
#ifdef BRYNET_PLATFORM_WINDOWS
        static CHAR temp[] = {0};
        static WSABUF in_buf = {0, temp};

        if (mPostRecvCheck)
        {
            return check_ret;
        }

        DWORD dwBytes = 0;
        DWORD flag = 0;
        const int ret = WSARecv(mSocket->getFD(),
                                &in_buf,
                                1,
                                &dwBytes,
                                &flag,
                                &(mOvlRecv.base),
                                0);
        if (ret == BRYNET_SOCKET_ERROR)
        {
            check_ret = (BRYNET_ERRNO == WSA_IO_PENDING);
        }

        if (check_ret)
        {
            mPostRecvCheck = true;
        }
#endif

        return check_ret;
    }

    bool checkWrite()
    {
        bool check_ret = true;
#ifdef BRYNET_PLATFORM_WINDOWS
        static WSABUF wsendbuf[1] = {{ NULL,
                                       0 }};

        if (mPostWriteCheck)
        {
            return check_ret;
        }

        DWORD send_len = 0;
        const int ret = WSASend(mSocket->getFD(),
                                wsendbuf,
                                1,
                                &send_len,
                                0,
                                &(mOvlSend.base),
                                0);
        if (ret == BRYNET_SOCKET_ERROR)
        {
            check_ret = (BRYNET_ERRNO == WSA_IO_PENDING);
        }

        if (check_ret)
        {
            mPostWriteCheck = true;
        }
#endif
        return check_ret;
    }

    void adjustReceiveBuffer()
    {
        if (!mRecvBuffer)
        {
            return;
        }

        if (buffer_getwritevalidcount(mRecvBuffer.get()) == 0 || buffer_getreadvalidcount(mRecvBuffer.get()) == 0)
        {
            buffer_adjustto_head(mRecvBuffer.get());
        }

        if (buffer_getreadvalidcount(mRecvBuffer.get()) == buffer_getsize(mRecvBuffer.get()))
        {
            growRecvBuffer();
        }
    }

    void recv()
    {
        bool must_close = false;
#ifdef BRYNET_USE_OPENSSL
        const bool notInSSL = (mSSL == nullptr);
#else
        const bool notInSSL = false;
#endif

        while (true)
        {
            adjustReceiveBuffer();

            const auto tryRecvLen = buffer_getwritevalidcount(mRecvBuffer.get());
            if (tryRecvLen == 0)
            {
#ifdef BRYNET_PLATFORM_WINDOWS
                checkRead();
#elif defined BRYNET_PLATFORM_LINUX || defined BRYNET_PLATFORM_DARWIN
                //force recheck IN-OUT Event
                recheckEvent();
#endif
                break;
            }

            int retlen = 0;
#ifdef BRYNET_USE_OPENSSL
            if (mSSL != nullptr)
            {
                retlen = SSL_read(mSSL,
                                  buffer_getwriteptr(mRecvBuffer.get()), tryRecvLen);
            }
            else
            {
                retlen = ::recv(mSocket->getFD(),
                                buffer_getwriteptr(mRecvBuffer.get()), tryRecvLen, 0);
            }
#else
            retlen = ::recv(mSocket->getFD(),
                            buffer_getwriteptr(mRecvBuffer.get()),
                            static_cast<int>(tryRecvLen),
                            0);
#endif

            if (retlen == 0)
            {
                must_close = true;
                break;
            }
            else if (retlen < 0)
            {
#ifdef BRYNET_USE_OPENSSL
                if ((mSSL != nullptr &&
                     SSL_get_error(mSSL, retlen) == SSL_ERROR_WANT_READ) ||
                    (BRYNET_ERRNO == BRYNET_EWOULDBLOCK))
                {
                    must_close = !checkRead();
                }
                else
                {
                    must_close = true;
                }
#else
                if (BRYNET_ERRNO != BRYNET_EWOULDBLOCK)
                {
                    must_close = true;
                }
                else
                {
                    must_close = !checkRead();
                }
#endif
                break;
            }

            mRecvData = true;
            buffer_addwritepos(mRecvBuffer.get(), static_cast<size_t>(retlen));

            if (notInSSL && retlen < static_cast<int>(tryRecvLen))
            {
                must_close = !checkRead();
                break;
            }
        }

        processRecvMessage();

        if (must_close)
        {
            procCloseInLoop();
        }
    }

    void flush()
    {
#ifdef BRYNET_PLATFORM_WINDOWS
        normalFlush();
#elif defined BRYNET_PLATFORM_LINUX || defined BRYNET_PLATFORM_DARWIN
#ifdef BRYNET_USE_OPENSSL
        if (mSSL != nullptr)
        {
            normalFlush();
        }
        else
        {
            quickFlush();
        }
#else
        quickFlush();
#endif
#endif
    }
    void normalFlush()
    {
#ifdef BRYNET_PLATFORM_WINDOWS
        static __declspec(thread) char* threadLocalSendBuf = nullptr;
#elif defined BRYNET_PLATFORM_LINUX || defined BRYNET_PLATFORM_DARWIN
        static __thread char* threadLocalSendBuf = nullptr;
#endif
        static const int SENDBUF_SIZE = 1024 * 32;
        if (threadLocalSendBuf == nullptr)
        {
            threadLocalSendBuf = static_cast<char*>(malloc(SENDBUF_SIZE));
        }

#ifdef BRYNET_USE_OPENSSL
        const bool notInSSL = (mSSL == nullptr);
#else
        const bool notInSSL = false;
#endif

        bool must_close = false;

        while (!mSendList.empty() && mCanWrite)
        {
            auto sendptr = threadLocalSendBuf;
            size_t wait_send_size = 0;

            for (auto it = mSendList.begin(); it != mSendList.end(); ++it)
            {
                auto& packet = *it;
                auto packetLeftBuf = (char*) (packet.data->data()) + packet.data->size() - packet.left;
                const auto packetLeftLen = packet.left;

                if ((wait_send_size + packetLeftLen) > SENDBUF_SIZE)
                {
                    if (it == mSendList.begin())
                    {
                        sendptr = packetLeftBuf;
                        wait_send_size = packetLeftLen;
                    }
                    break;
                }

                memcpy(static_cast<void*>(sendptr + wait_send_size), static_cast<void*>(packetLeftBuf), packetLeftLen);
                wait_send_size += packetLeftLen;
            }

            if (wait_send_size == 0)
            {
                break;
            }

            int send_retlen = 0;
#ifdef BRYNET_USE_OPENSSL
            if (mSSL != nullptr)
            {
                send_retlen = SSL_write(mSSL, sendptr, wait_send_size);
            }
            else
            {
                send_retlen = ::send(mSocket->getFD(), sendptr, wait_send_size, 0);
            }
#else
            send_retlen = ::send(mSocket->getFD(), sendptr, static_cast<int>(wait_send_size), 0);
#endif
            if (send_retlen <= 0)
            {

#ifdef BRYNET_USE_OPENSSL
                if ((mSSL != nullptr && SSL_get_error(mSSL, send_retlen) == SSL_ERROR_WANT_WRITE) ||
                    (BRYNET_ERRNO == BRYNET_EWOULDBLOCK))
                {
                    mCanWrite = false;
                    must_close = !checkWrite();
                }
                else
                {
                    must_close = true;
                }
#else
                if (BRYNET_ERRNO == BRYNET_EWOULDBLOCK)
                {
                    mCanWrite = false;
                    must_close = !checkWrite();
                }
                else
                {
                    must_close = true;
                }
#endif
                break;
            }

            auto tmp_len = static_cast<size_t>(send_retlen);
            for (auto it = mSendList.begin(); it != mSendList.end();)
            {
                auto& packet = *it;
                if (packet.left > tmp_len)
                {
                    packet.left -= tmp_len;
                    break;
                }

                tmp_len -= packet.left;
                if (packet.mCompleteCallback != nullptr)
                {
                    pedingCallbacks.push_back(std::move(packet.mCompleteCallback));
                }
                mSendingMsgSize -= packet.data->size();
                it = mSendList.erase(it);
            }
            for (auto&& callback : pedingCallbacks)
            {
                callback();
            }
            pedingCallbacks.clear();

            if (notInSSL && static_cast<size_t>(send_retlen) != wait_send_size)
            {
                mCanWrite = false;
                must_close = !checkWrite();
                break;
            }
        }

        if (must_close)
        {
            procCloseInLoop();
        }
    }
#if defined BRYNET_PLATFORM_LINUX || defined BRYNET_PLATFORM_DARWIN
    void quickFlush()
    {
#ifndef MAX_IOVEC
        constexpr size_t MAX_IOVEC = 1024;
#endif

        struct iovec iov[MAX_IOVEC];
        bool must_close = false;

        while (!mSendList.empty() && mCanWrite)
        {
            size_t num = 0;
            size_t ready_send_len = 0;
            for (const auto& p : mSendList)
            {
                iov[num].iov_base = (void*) (static_cast<const char*>(p.data->data()) + p.data->size() - p.left);
                iov[num].iov_len = p.left;
                ready_send_len += p.left;

                num++;
                if (num >= MAX_IOVEC)
                {
                    break;
                }
            }

            if (num == 0)
            {
                break;
            }

            const int send_len = writev(mSocket->getFD(), iov, static_cast<int>(num));
            if (send_len <= 0)
            {
                if (BRYNET_ERRNO == BRYNET_EWOULDBLOCK)
                {
                    mCanWrite = false;
                    must_close = !checkWrite();
                }
                else
                {
                    must_close = true;
                }
                break;
            }

            auto tmp_len = static_cast<size_t>(send_len);
            for (auto it = mSendList.begin(); it != mSendList.end();)
            {
                PendingPacket& b = *it;
                if (b.left > tmp_len)
                {
                    b.left -= tmp_len;
                    break;
                }

                tmp_len -= b.left;
                if (b.mCompleteCallback != nullptr)
                {
                    pedingCallbacks.push_back(std::move(b.mCompleteCallback));
                }
                mSendingMsgSize -= b.data->size();
                it = mSendList.erase(it);
            }
            for (auto&& callback : pedingCallbacks)
            {
                callback();
            }
            pedingCallbacks.clear();

            if (static_cast<size_t>(send_len) != ready_send_len)
            {
                mCanWrite = false;
                must_close = !checkWrite();
                break;
            }
        }

        if (must_close)
        {
            procCloseInLoop();
        }
    }
#endif

    void onClose() override
    {
        if (mAlreadyClose)
        {
            return;
        }
        mAlreadyClose = true;

#if defined BRYNET_PLATFORM_LINUX || defined BRYNET_PLATFORM_DARWIN
        unregisterPollerEvent();
#endif

        assert(mEnterCallback == nullptr);
        auto callBack = mDisConnectCallback;
        auto sharedThis = shared_from_this();
        auto eventLoop = mEventLoop;
        std::shared_ptr<TcpSocket> socket = std::move(const_cast<TcpSocket::Ptr&&>(mSocket));
        mEventLoop->runFunctorAfterLoop([callBack,
                                         sharedThis,
                                         eventLoop,
                                         socket]() {
            if (callBack != nullptr)
            {
                callBack(sharedThis);
            }
            auto tmp = eventLoop->getTcpConnection(socket->getFD());
            assert(tmp == sharedThis);
            if (tmp == sharedThis)
            {
                eventLoop->removeTcpConnection(socket->getFD());
            }
        });

        mCanWrite = false;
        mDataCallback = nullptr;
        mDisConnectCallback = nullptr;
        mHighWaterCallback = nullptr;
        mRecvBuffer = nullptr;
        mSendList.clear();
    }

    void procCloseInLoop()
    {
        mCanWrite = false;
#ifdef BRYNET_PLATFORM_WINDOWS
        if (mPostWriteCheck || mPostRecvCheck)
        {
            if (mPostClose)
            {
                return;
            }
            mPostClose = true;
            //windows下立即关闭socket可能导致fd被另外的TcpConnection重用,而导致此对象在IOCP返回相关完成结果时内存已经释放
            if (mPostRecvCheck)
            {
                CancelIoEx(HANDLE(mSocket->getFD()), &mOvlRecv.base);
            }
            if (mPostWriteCheck)
            {
                CancelIoEx(HANDLE(mSocket->getFD()), &mOvlSend.base);
            }
        }
        else
        {
            onClose();
        }
#elif defined BRYNET_PLATFORM_LINUX
        onClose();
#elif defined BRYNET_PLATFORM_DARWIN
        onClose();
#endif
    }

    void procShutdownInLoop()
    {
        mCanWrite = false;
        if (mSocket != nullptr)
        {
#ifdef BRYNET_PLATFORM_WINDOWS
            shutdown(mSocket->getFD(), SD_SEND);
#elif defined BRYNET_PLATFORM_LINUX || defined BRYNET_PLATFORM_DARWIN
            shutdown(mSocket->getFD(), SHUT_WR);
#endif
        }
    }

    void runAfterFlush()
    {
        if (!mIsPostFlush && !mSendList.empty() && mCanWrite)
        {
            auto sharedThis = shared_from_this();
            mEventLoop->runFunctorAfterLoop([sharedThis, this]() {
                mIsPostFlush = false;
                flush();
            });

            mIsPostFlush = true;
        }
    }
#if defined BRYNET_PLATFORM_LINUX || defined BRYNET_PLATFORM_DARWIN
    void recheckEvent()
    {
#ifdef BRYNET_PLATFORM_LINUX
        struct epoll_event ev = {0,
                                 {
                                     nullptr
                                 }};
        ev.events = EPOLLET | EPOLLIN | EPOLLOUT | EPOLLRDHUP;
        ev.data.ptr = (Channel*) (this);
        epoll_ctl(mEventLoop->getEpollHandle(), EPOLL_CTL_MOD, mSocket->getFD(), &ev);
#elif defined BRYNET_PLATFORM_DARWIN
        struct kevent ev[2];
        memset(&ev, 0, sizeof(ev));
        int n = 0;
        EV_SET(&ev[n++], mSocket->getFD(), EVFILT_READ, EV_ENABLE, 0, 0, (Channel*) (this));
        EV_SET(&ev[n++], mSocket->getFD(), EVFILT_WRITE, EV_ENABLE, 0, 0, (Channel*) (this));

        struct timespec now = {0, 0};
        kevent(mEventLoop->getKqueueHandle(), ev, n, NULL, 0, &now);
#endif
    }
    void unregisterPollerEvent()
    {
#ifdef BRYNET_PLATFORM_LINUX
        struct epoll_event ev = {0,
                                 {
                                     nullptr
                                 }};
        epoll_ctl(mEventLoop->getEpollHandle(), EPOLL_CTL_DEL, mSocket->getFD(), &ev);
#elif defined BRYNET_PLATFORM_DARWIN
        struct kevent ev[2];
        memset(&ev, 0, sizeof(ev));
        int n = 0;
        EV_SET(&ev[n++], mSocket->getFD(), EVFILT_READ, EV_DELETE, 0, 0, NULL);
        EV_SET(&ev[n++], mSocket->getFD(), EVFILT_WRITE, EV_DELETE, 0, 0, NULL);

        struct timespec now = {0, 0};
        kevent(mEventLoop->getKqueueHandle(), ev, n, NULL, 0, &now);
#endif
    }
#endif
#ifdef BRYNET_USE_OPENSSL
    bool processSSLHandshake()
    {
        if (mIsHandsharked)
        {
            return true;
        }

        bool mustClose = false;
        int ret = 0;

        if (mSSLCtx != nullptr)
        {
            ret = SSL_connect(mSSL);
        }
        else
        {
            ret = SSL_accept(mSSL);
        }

        if (ret == 1)
        {
            mIsHandsharked = true;
            if (checkRead())
            {
                causeEnterCallback();
            }
            else
            {
                mustClose = true;
            }
        }
        else if (ret == 0)
        {
            mustClose = true;
        }
        else if (ret < 0)
        {
            int err = SSL_get_error(mSSL, ret);
            if (err == SSL_ERROR_WANT_WRITE || err == SSL_ERROR_WANT_READ)
            {
                if (!checkRead())
                {
                    mustClose = true;
                }
            }
            else
            {
                mustClose = true;
            }
        }

        if (mustClose)
        {
            causeEnterCallback();
            procCloseInLoop();
            return false;
        }
        return true;
    }
#endif
    void causeEnterCallback()
    {
        assert(mEventLoop->isInLoopThread());
        if (mEventLoop->isInLoopThread() && mEnterCallback != nullptr)
        {
            auto tmp = mEnterCallback;
            mEnterCallback = nullptr;
            tmp(shared_from_this());
        }
    }

    void processRecvMessage()
    {
        if (mDataCallback != nullptr && buffer_getreadvalidcount(mRecvBuffer.get()) > 0)
        {
            auto reader = brynet::base::BasePacketReader(buffer_getreadptr(mRecvBuffer.get()),
                                                         buffer_getreadvalidcount(mRecvBuffer.get()), false);
            mDataCallback(reader);
            const auto consumedLen = reader.savedPos();
            assert(consumedLen <= reader.size());
            if (consumedLen <= reader.size())
            {
                buffer_addreadpos(mRecvBuffer.get(), consumedLen);
            }
        }
    }

    template<typename CallbackType, typename Arg>
    static void verifyArgType(const CallbackType&, void (CallbackType::*)(Arg&) const)
    {
        static_assert(std::is_reference<Arg&>::value, "arg must be reference type");
        static_assert(!std::is_const<typename std::remove_const<Arg>::type>::value, "arg can't be const type");
    }

private:
#ifdef BRYNET_PLATFORM_WINDOWS
    struct port::Win::OverlappedExt mOvlRecv;
    struct port::Win::OverlappedExt mOvlSend;

    bool mPostRecvCheck;
    bool mPostWriteCheck;
    bool mPostClose;
#endif
    const std::string mIP;
    const TcpSocket::Ptr mSocket;
    const EventLoop::Ptr mEventLoop;
    bool mCanWrite;
    bool mAlreadyClose;

    class BufferDeleter
    {
    public:
        void operator()(struct brynet::base::buffer_s* ptr) const
        {
            brynet::base::buffer_delete(ptr);
        }
    };
    std::unique_ptr<struct brynet::base::buffer_s, BufferDeleter> mRecvBuffer;
    double mCurrentTanhXDiff = 0;
    size_t mRecvBuffOriginSize = 0;
    const size_t mMaxRecvBufferSize;

    struct PendingPacket
    {
        SendableMsg::Ptr data;
        size_t left;
        PacketSendedCallback mCompleteCallback;
    };

    std::vector<PacketSendedCallback> pedingCallbacks;

    using PacketListType = std::deque<PendingPacket>;
    PacketListType mSendList;
    size_t mSendingMsgSize;

    EnterCallback mEnterCallback;
    DataCallback mDataCallback;
    DisconnectedCallback mDisConnectCallback;
    HighWaterCallback mHighWaterCallback;
    size_t mHighWaterSize;

    bool mIsPostFlush;

#ifdef BRYNET_USE_OPENSSL
    SSL_CTX* mSSLCtx;
    SSL* mSSL;
    bool mIsHandsharked;
#endif
    bool mRecvData;
    std::chrono::nanoseconds mCheckTime{};
    brynet::base::Timer::WeakPtr mTimer;
};

}}// namespace brynet::net
