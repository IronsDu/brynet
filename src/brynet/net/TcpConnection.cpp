#include <cassert>
#include <cstring>

#include <brynet/net/SocketLibFunction.h>
#include <brynet/net/EventLoop.h>
#include <brynet/utils/buffer.h>

#include <brynet/net/TcpConnection.h>

namespace brynet { namespace net {

    TcpConnection::TcpConnection(TcpSocket::Ptr socket,
        size_t maxRecvBufferSize,
        EnterCallback enterCallback,
        EventLoop::Ptr eventLoop) BRYNET_NOEXCEPT
        :
#if defined PLATFORM_WINDOWS
    mOvlRecv(EventLoop::OverlappedType::OverlappedRecv),
        mOvlSend(EventLoop::OverlappedType::OverlappedSend),
        mPostClose(false),
#endif
        mIP(socket->getRemoteIP()),
        mSocket(std::move(socket)),
        mEventLoop(std::move(eventLoop)),
        mAlreadyClose(false),
        mMaxRecvBufferSize(maxRecvBufferSize)
    {
        mRecvData = false;
        mCheckTime = std::chrono::steady_clock::duration::zero();
        mIsPostFlush = false;

        mCanWrite = true;

#ifdef PLATFORM_WINDOWS
        mPostRecvCheck = false;
        mPostWriteCheck = false;
#endif
        growRecvBuffer();

#ifdef USE_OPENSSL
        mSSLCtx = nullptr;
        mSSL = nullptr;
        mIsHandsharked = false;
#endif
        mEnterCallback = std::move(enterCallback);
    }

    TcpConnection::~TcpConnection() BRYNET_NOEXCEPT
    {
#ifdef USE_OPENSSL
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

        // 如果构造TcpConnection发生异常则mSocket也可能为nullptr
        assert(mSocket != nullptr);

        if (mTimer.lock())
        {
            mTimer.lock()->cancel();
        }
    }

    TcpConnection::Ptr TcpConnection::Create(TcpSocket::Ptr socket,
        size_t maxRecvBufferSize,
        EnterCallback enterCallback,
        EventLoop::Ptr eventLoop)
    {
        struct make_shared_enabler : public TcpConnection
        {
            make_shared_enabler(TcpSocket::Ptr socket,
                size_t maxRecvBufferSize,
                EnterCallback enterCallback,
                EventLoop::Ptr eventLoop)
                :
                TcpConnection(std::move(socket), maxRecvBufferSize, std::move(enterCallback), std::move(eventLoop))
            {}
        };
        return std::make_shared<make_shared_enabler>(
            std::move(socket),
            maxRecvBufferSize,
            std::move(enterCallback),
            std::move(eventLoop));
    }

    bool TcpConnection::onEnterEventLoop()
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
        assert(findRet == nullptr);

#ifdef USE_OPENSSL
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

    const EventLoop::Ptr& TcpConnection::getEventLoop() const
    {
        return mEventLoop;
    }

    void TcpConnection::send(const char* buffer, size_t len, const PacketSendedCallback& callback)
    {
        send(makePacket(buffer, len), callback);
    }

    void TcpConnection::send(const PacketPtr& packet, const PacketSendedCallback& callback)
    {
        auto packetCapture = packet;
        auto callbackCapture = callback;
        auto sharedThis = shared_from_this();
        mEventLoop->pushAsyncFunctor([sharedThis, packetCapture, callbackCapture]() mutable {
            const auto len = packetCapture->size();
            sharedThis->mSendList.push_back({ std::move(packetCapture), len, std::move(callbackCapture) });
            sharedThis->runAfterFlush();
        });
    }

    void TcpConnection::sendInLoop(const PacketPtr& packet, const PacketSendedCallback& callback)
    {
        assert(mEventLoop->isInLoopThread());
        if (mEventLoop->isInLoopThread())
        {
            const auto len = packet->size();
            mSendList.push_back({ packet, len, callback });
            runAfterFlush();
        }
    }

    void TcpConnection::canRecv()
    {
#ifdef PLATFORM_WINDOWS
        mPostRecvCheck = false;
        if (mPostClose)
        {
            if (!mPostWriteCheck)
            {
                onClose();
            }
            return;
        }
#endif

#ifdef USE_OPENSSL
        if (!mIsHandsharked && mSSL != nullptr)
        {
            if (!processSSLHandshake() || !mIsHandsharked)
            {
                return;
            }
        }
#endif

        recv();
    }

    void TcpConnection::canSend()
    {
#ifdef PLATFORM_WINDOWS
        mPostWriteCheck = false;
        if (mPostClose)
        {
            if (!mPostRecvCheck)
            {
                onClose();
            }
            return;
        }
#else
        removeCheckWrite();
#endif
        mCanWrite = true;

#ifdef USE_OPENSSL
        if (!mIsHandsharked && mSSL != nullptr)
        {
            if (!processSSLHandshake() || !mIsHandsharked)
            {
                return;
            }
        }
#endif

        runAfterFlush();
    }

    void TcpConnection::runAfterFlush()
    {
        if (!mIsPostFlush && !mSendList.empty() && mCanWrite)
        {
            auto sharedThis = shared_from_this();
            mEventLoop->pushAfterLoopFunctor([sharedThis]() {
                sharedThis->mIsPostFlush = false;
                sharedThis->flush();
            });

            mIsPostFlush = true;
        }
    }

    void TcpConnection::recv()
    {
        bool must_close = false;
#ifdef USE_OPENSSL
        const bool notInSSL = (mSSL == nullptr);
#else
        const bool notInSSL = false;
#endif

        while (true)
        {
            const auto tryRecvLen = ox_buffer_getwritevalidcount(mRecvBuffer.get());
            if (tryRecvLen == 0)
            {
                break;
            }

            int retlen = 0;
#ifdef USE_OPENSSL
            if (mSSL != nullptr)
            {
                retlen = SSL_read(mSSL, ox_buffer_getwriteptr(mRecvBuffer.get()), tryRecvLen);
            }
            else
            {
                retlen = ::recv(mSocket->getFD(), ox_buffer_getwriteptr(mRecvBuffer.get()), tryRecvLen, 0);
            }
#else
            retlen = ::recv(mSocket->getFD(), ox_buffer_getwriteptr(mRecvBuffer.get()), static_cast<int>(tryRecvLen), 0);
#endif

            if (retlen == 0)
            {
                must_close = true;
                break;
            }
            else if (retlen < 0)
            {
#ifdef USE_OPENSSL
                if ((mSSL != nullptr && SSL_get_error(mSSL, retlen) == SSL_ERROR_WANT_READ) ||
                    (sErrno == S_EWOULDBLOCK))
                {
                    must_close = !checkRead();
                }
                else
                {
                    must_close = true;
                }
#else
                if (sErrno != S_EWOULDBLOCK)
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

            ox_buffer_addwritepos(mRecvBuffer.get(), retlen);
            if (ox_buffer_getreadvalidcount(mRecvBuffer.get()) == ox_buffer_getsize(mRecvBuffer.get()))
            {
                growRecvBuffer();
            }

            if (mDataCallback != nullptr)
            {
                mRecvData = true;
                auto proclen = mDataCallback(ox_buffer_getreadptr(mRecvBuffer.get()),
                    ox_buffer_getreadvalidcount(mRecvBuffer.get()));
                assert(proclen <= ox_buffer_getreadvalidcount(mRecvBuffer.get()));
                if (proclen <= ox_buffer_getreadvalidcount(mRecvBuffer.get()))
                {
                    ox_buffer_addreadpos(mRecvBuffer.get(), proclen);
                }
                else
                {
                    break;
                }
            }

            if (ox_buffer_getwritevalidcount(mRecvBuffer.get()) == 0 || ox_buffer_getreadvalidcount(mRecvBuffer.get()) == 0)
            {
                ox_buffer_adjustto_head(mRecvBuffer.get());
            }

            if (notInSSL && retlen < static_cast<int>(tryRecvLen))
            {
                must_close = !checkRead();
                break;
            }
        }

        if (must_close)
        {
            procCloseInLoop();
        }
    }

    void TcpConnection::flush()
    {
#ifdef PLATFORM_WINDOWS
        normalFlush();
#else
#ifdef USE_OPENSSL
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

#ifdef PLATFORM_WINDOWS
    __declspec(thread) char* threadLocalSendBuf = nullptr;
#else
    __thread char* threadLocalSendBuf = nullptr;
#endif

    void TcpConnection::normalFlush()
    {
        static  const   int SENDBUF_SIZE = 1024 * 32;
        if (threadLocalSendBuf == nullptr)
        {
            threadLocalSendBuf = (char*)malloc(SENDBUF_SIZE);
        }

#ifdef USE_OPENSSL
        const bool notInSSL = (mSSL == nullptr);
#else
        const bool notInSSL = false;
#endif

        bool must_close = false;

        while (!mSendList.empty() && mCanWrite)
        {
            char* sendptr = threadLocalSendBuf;
            size_t     wait_send_size = 0;

            for (auto it = mSendList.begin(); it != mSendList.end(); ++it)
            {
                auto& packet = *it;
                auto packetLeftBuf = (char*)(packet.data->c_str() + (packet.data->size() - packet.left));
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

                memcpy(sendptr + wait_send_size, packetLeftBuf, packetLeftLen);
                wait_send_size += packetLeftLen;
            }

            if (wait_send_size == 0)
            {
                break;
            }

            int send_retlen = 0;
#ifdef USE_OPENSSL
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

#ifdef USE_OPENSSL
                if ((mSSL != nullptr && SSL_get_error(mSSL, send_retlen) == SSL_ERROR_WANT_WRITE) ||
                    (sErrno == S_EWOULDBLOCK))
                {
                    mCanWrite = false;
                    must_close = !checkWrite();
                }
                else
                {
                    must_close = true;
                }
#else
                if (sErrno == S_EWOULDBLOCK)
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
                    (packet.mCompleteCallback)();
                }
                it = mSendList.erase(it);
            }

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

    void TcpConnection::quickFlush()
    {
#ifdef PLATFORM_LINUX
#ifndef MAX_IOVEC
#define  MAX_IOVEC 1024
#endif

        struct iovec iov[MAX_IOVEC];
        bool must_close = false;

        while (!mSendList.empty() && mCanWrite)
        {
            int num = 0;
            size_t ready_send_len = 0;
            for (const auto& p : mSendList)
            {
                iov[num].iov_base = (void*)(p.data->c_str() + (p.data->size() - p.left));
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

            const int send_len = writev(mSocket->getFD(), iov, num);
            if (send_len <= 0)
            {
                if (sErrno == S_EWOULDBLOCK)
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
                    b.mCompleteCallback();
                }
                it = mSendList.erase(it);
            }

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
#endif
    }

    void TcpConnection::procCloseInLoop()
    {
        mCanWrite = false;
#ifdef PLATFORM_WINDOWS
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
#else
        struct epoll_event ev = { 0, { nullptr } };
        epoll_ctl(mEventLoop->getEpollHandle(), EPOLL_CTL_DEL, mSocket->getFD(), &ev);
        onClose();
#endif
    }

    void TcpConnection::procShutdownInLoop()
    {
        mCanWrite = false;
        assert(mSocket != nullptr);
        if (mSocket != nullptr)
        {
#ifdef PLATFORM_WINDOWS
            shutdown(mSocket->getFD(), SD_SEND);
#else
            shutdown(mSocket->getFD(), SHUT_WR);
#endif
        }
    }

    void TcpConnection::onClose()
    {
        if (mAlreadyClose)
        {
            return;
        }
        mAlreadyClose = true;

        assert(mEnterCallback == nullptr);
        auto callBack = mDisConnectCallback;
        auto sharedThis = shared_from_this();
        auto eventLoop = mEventLoop;
        auto fd = mSocket->getFD();
        mEventLoop->pushAfterLoopFunctor([callBack,
            sharedThis,
            eventLoop,
            fd]() {
            if (callBack != nullptr)
            {
                callBack(sharedThis);
            }
            auto tmp = eventLoop->getTcpConnection(fd);
            assert(tmp == sharedThis);
            if (tmp == sharedThis)
            {
                eventLoop->removeTcpConnection(fd);
            }
        });
        mDisConnectCallback = nullptr;
        mDataCallback = nullptr;
    }

    bool TcpConnection::checkRead()
    {
        bool check_ret = true;
#ifdef PLATFORM_WINDOWS
        static CHAR temp[] = { 0 };
        static WSABUF  in_buf = { 0, temp };

        if (mPostRecvCheck)
        {
            return check_ret;
        }

        DWORD   dwBytes = 0;
        DWORD   flag = 0;
        const int ret = WSARecv(mSocket->getFD(), &in_buf, 1, &dwBytes, &flag, &(mOvlRecv.base), 0);
        if (ret == SOCKET_ERROR)
        {
            check_ret = (sErrno == WSA_IO_PENDING);
        }

        if (check_ret)
        {
            mPostRecvCheck = true;
        }
#endif

        return check_ret;
    }

    bool    TcpConnection::checkWrite()
    {
        bool check_ret = true;
#ifdef PLATFORM_WINDOWS
        static WSABUF wsendbuf[1] = { {NULL, 0} };

        if (mPostWriteCheck)
        {
            return check_ret;
        }

        DWORD send_len = 0;
        const int ret = WSASend(mSocket->getFD(), wsendbuf, 1, &send_len, 0, &(mOvlSend.base), 0);
        if (ret == SOCKET_ERROR)
        {
            check_ret = (sErrno == WSA_IO_PENDING);
        }

        if (check_ret)
        {
            mPostWriteCheck = true;
        }
#else
        struct epoll_event ev = { 0, { nullptr } };
        ev.events = EPOLLET | EPOLLIN | EPOLLOUT | EPOLLRDHUP;
        ev.data.ptr = (Channel*)(this);
        epoll_ctl(mEventLoop->getEpollHandle(), EPOLL_CTL_MOD, mSocket->getFD(), &ev);
#endif

        return check_ret;
    }

#ifdef PLATFORM_LINUX
    void    TcpConnection::removeCheckWrite()
    {
        struct epoll_event ev = { 0, { nullptr } };
        ev.events = EPOLLET | EPOLLIN | EPOLLRDHUP;
        ev.data.ptr = (Channel*)(this);
        epoll_ctl(mEventLoop->getEpollHandle(), EPOLL_CTL_MOD, mSocket->getFD(), &ev);
    }
#endif

    void TcpConnection::setDataCallback(DataCallback cb)
    {
        auto sharedThis = shared_from_this();
        mEventLoop->pushAsyncFunctor([sharedThis, cb]() mutable {
            sharedThis->mDataCallback = std::move(cb);
        });
    }

    void TcpConnection::setDisConnectCallback(DisconnectedCallback cb)
    {
        auto sharedThis = shared_from_this();
        mEventLoop->pushAsyncFunctor([sharedThis, cb]() mutable {
            sharedThis->mDisConnectCallback = std::move(cb);
        });
    }

    const static size_t GROW_BUFFER_SIZE = 1024;

    void TcpConnection::growRecvBuffer()
    {
        if (mRecvBuffer == nullptr)
        {
            mRecvBuffer.reset(ox_buffer_new(std::min<size_t>(16 * 1024, mMaxRecvBufferSize)));
        }
        else
        {
            const auto NewSize = ox_buffer_getsize(mRecvBuffer.get()) + GROW_BUFFER_SIZE;
            if (NewSize > mMaxRecvBufferSize)
            {
                return;
            }
            std::unique_ptr<struct buffer_s, BufferDeleter> newBuffer(ox_buffer_new(NewSize));
            ox_buffer_write(newBuffer.get(),
                ox_buffer_getreadptr(mRecvBuffer.get()),
                ox_buffer_getreadvalidcount(mRecvBuffer.get()));
            mRecvBuffer = std::move(newBuffer);
        }
    }
    void TcpConnection::pingCheck()
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

    void TcpConnection::startPingCheckTimer()
    {
        if (!mTimer.lock() && mCheckTime != std::chrono::steady_clock::duration::zero())
        {
            std::weak_ptr<TcpConnection> weakedThis = shared_from_this();
            mTimer = mEventLoop->runAfter(mCheckTime, [weakedThis]() {
                auto sharedThis = weakedThis.lock();
                if (sharedThis != nullptr)
                {
                    sharedThis->pingCheck();
                }
            });
        }
    }

    void TcpConnection::setHeartBeat(std::chrono::nanoseconds checkTime)
    {
        auto sharedThis = shared_from_this();
        mEventLoop->pushAsyncFunctor([sharedThis, checkTime]() {
            if (sharedThis->mTimer.lock() != nullptr)
            {
                sharedThis->mTimer.lock()->cancel();
                sharedThis->mTimer.reset();
            }

            sharedThis->mCheckTime = checkTime;
            sharedThis->startPingCheckTimer();
        });

    }

    void TcpConnection::postDisConnect()
    {
        auto sharedThis = shared_from_this();
        mEventLoop->pushAsyncFunctor([sharedThis]() {
            sharedThis->procCloseInLoop();
        });
    }

    void TcpConnection::postShutdown()
    {
        auto sharedThis = shared_from_this();
        mEventLoop->pushAsyncFunctor([sharedThis]() {
            sharedThis->mEventLoop->pushAfterLoopFunctor([sharedThis]() {
                sharedThis->procShutdownInLoop();
            });
        });
    }

    void TcpConnection::setUD(BrynetAny value)
    {
        mUD = std::move(value);
    }

    const BrynetAny& TcpConnection::getUD() const
    {
        return mUD;
    }

    const std::string& TcpConnection::getIP() const
    {
        return mIP;
    }
#ifdef USE_OPENSSL
    bool TcpConnection::initAcceptSSL(SSL_CTX* ctx)
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

    bool TcpConnection::initConnectSSL()
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

    bool TcpConnection::processSSLHandshake()
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

    TcpConnection::PacketPtr TcpConnection::makePacket(const char* buffer, size_t len)
    {
        return std::make_shared<std::string>(buffer, len);
    }

    void TcpConnection::causeEnterCallback()
    {
        assert(mEventLoop->isInLoopThread());
        if (mEventLoop->isInLoopThread() && mEnterCallback != nullptr)
        {
            auto tmp = mEnterCallback;
            mEnterCallback = nullptr;
            tmp(shared_from_this());
        }
    }

} }
