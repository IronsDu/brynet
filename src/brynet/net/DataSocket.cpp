#include <cassert>
#include <cstring>

#include <brynet/net/SocketLibFunction.h>
#include <brynet/net/EventLoop.h>
#include <brynet/utils/buffer.h>

#include <brynet/net/DataSocket.h>

using namespace brynet::net;

DataSocket::DataSocket(sock fd, size_t maxRecvBufferSize) noexcept
#if defined PLATFORM_WINDOWS
    : mOvlRecv(EventLoop::OLV_VALUE::OVL_RECV), mOvlSend(EventLoop::OLV_VALUE::OVL_SEND)
#endif
{
    mMaxRecvBufferSize = maxRecvBufferSize;
    mRecvData = false;
    mCheckTime = -1;
    mIsPostFinalClose = false;
    mIsPostFlush = false;
    mFD = fd;

    mCanWrite = true;

#ifdef PLATFORM_WINDOWS
    mPostRecvCheck = false;
    mPostWriteCheck = false;
#endif
    mRecvBuffer = nullptr;
    growRecvBuffer();

#ifdef USE_OPENSSL
    mSSLCtx = nullptr;
    mSSL = nullptr;
    mIsHandsharked = false;
#endif
}

DataSocket::~DataSocket() noexcept
{
    assert(mFD == SOCKET_ERROR);

    if (mRecvBuffer != nullptr)
    {
        ox_buffer_delete(mRecvBuffer);
        mRecvBuffer = nullptr;
    }

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

    closeSocket();

    if (mTimer.lock())
    {
        mTimer.lock()->cancel();
    }
}

bool DataSocket::onEnterEventLoop(const EventLoop::PTR& eventLoop)
{
    assert(eventLoop->isInLoopThread());
    if (!eventLoop->isInLoopThread())
    {
        return false;
    }

    assert(mEventLoop == nullptr);
    if (mEventLoop != nullptr)
    {
        return false;
    }

    mEventLoop = eventLoop;

    if (!ox_socket_nonblock(mFD))
    {
        closeSocket();
        return false;
    }
    if (!mEventLoop->linkChannel(mFD, this))
    {
        closeSocket();
        return false;
    }

    bool ret = false;
    auto isUseSSL = false;

#ifdef USE_OPENSSL
    isUseSSL = mSSL != nullptr;
#endif

    if (isUseSSL)
    {
#ifdef USE_OPENSSL
        processSSLHandshake();
        ret = true;
#endif
    }
    else
    {
        if (checkRead())
        {
            if (mEnterCallback != nullptr)
            {
                mEnterCallback(this);
                mEnterCallback = nullptr;
            }
            ret = true;
        }
        else
        {
            closeSocket();
        }
    }

    return ret;
}

void DataSocket::send(const char* buffer, size_t len, const PACKED_SENDED_CALLBACK& callback)
{
    sendPacket(makePacket(buffer, len), callback);
}

void DataSocket::sendPacket(const PACKET_PTR& packet, const PACKED_SENDED_CALLBACK& callback)
{
    mEventLoop->pushAsyncProc([this, packetCapture = packet, callbackCapture = callback](){
        auto len = packetCapture->size();
        mSendList.push_back({ std::move(packetCapture), len, std::move(callbackCapture) });
        runAfterFlush();
    });
}

void DataSocket::sendPacketInLoop(const PACKET_PTR& packet, const PACKED_SENDED_CALLBACK& callback)
{
    assert(mEventLoop->isInLoopThread());
    if (mEventLoop->isInLoopThread())
    {
        auto len = packet->size();
        mSendList.push_back({ packet, len, callback });
        runAfterFlush();
    }
}

void DataSocket::canRecv()
{
#ifdef PLATFORM_WINDOWS
    mPostRecvCheck = false;
    if (mFD == SOCKET_ERROR && !mPostWriteCheck)
    {
        onClose();
        return;
    }
#endif

#ifdef USE_OPENSSL
    if (!mIsHandsharked && mSSL != nullptr)
    {
        processSSLHandshake();
        return;
    }
#endif

    recv();
}

void DataSocket::canSend()
{
#ifdef PLATFORM_WINDOWS
    mPostWriteCheck = false;
    if (mFD == SOCKET_ERROR && !mPostRecvCheck)
    {
        onClose();
        return;
    }
#else
    removeCheckWrite();
#endif
    mCanWrite = true;

#ifdef USE_OPENSSL
    if (!mIsHandsharked && mSSL != nullptr)
    {
        processSSLHandshake();
        return;
    }
#endif

    runAfterFlush();
}

void DataSocket::runAfterFlush()
{
    if (!mIsPostFlush && !mSendList.empty() && mFD != SOCKET_ERROR)
    {
        mEventLoop->pushAfterLoopProc([=](){
            mIsPostFlush = false;
            flush();
        });

        mIsPostFlush = true;
    }
}

void DataSocket::recv()
{
    bool must_close = false;

    while (mFD != SOCKET_ERROR)
    {
        const auto tryRecvLen = ox_buffer_getwritevalidcount(mRecvBuffer);
        if (tryRecvLen == 0)
        {
            break;
        }

        int retlen = 0;
#ifdef USE_OPENSSL
        if (mSSL != nullptr)
        {
            retlen = SSL_read(mSSL, ox_buffer_getwriteptr(mRecvBuffer), tryRecvLen);
        }
        else
        {
            retlen = ::recv(mFD, ox_buffer_getwriteptr(mRecvBuffer), tryRecvLen, 0);
        }
#else
        retlen = ::recv(mFD, ox_buffer_getwriteptr(mRecvBuffer), static_cast<int>(tryRecvLen), 0);
#endif

        if (retlen < 0)
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
            if (sErrno == S_EWOULDBLOCK)
            {
                must_close = !checkRead();
            }
            else
            {
                must_close = true;
            }
#endif
            break;
        }
        else if (retlen == 0)
        {
            must_close = true;
            break;
        }
        else
        {
            ox_buffer_addwritepos(mRecvBuffer, retlen);
            if (ox_buffer_getreadvalidcount(mRecvBuffer) == ox_buffer_getsize(mRecvBuffer))
            {
                growRecvBuffer();
            }

            if (mDataCallback != nullptr)
            {
                mRecvData = true;
                auto proclen = mDataCallback(this, ox_buffer_getreadptr(mRecvBuffer), ox_buffer_getreadvalidcount(mRecvBuffer));
                assert(proclen <= ox_buffer_getreadvalidcount(mRecvBuffer));
                if (proclen <= ox_buffer_getreadvalidcount(mRecvBuffer))
                {
                    ox_buffer_addreadpos(mRecvBuffer, proclen);
                }
                else
                {
                    break;
                }
            }
        }

        if (ox_buffer_getwritevalidcount(mRecvBuffer) == 0 || ox_buffer_getreadvalidcount(mRecvBuffer) == 0)
        {
            ox_buffer_adjustto_head(mRecvBuffer);
        }
    }

    if (must_close)
    {
        procCloseInLoop();
    }
}

void DataSocket::flush()
{
    if (mCanWrite && mFD != SOCKET_ERROR)
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
}

#ifdef PLATFORM_WINDOWS
__declspec(thread) char* threadLocalSendBuf = nullptr;
#else
__thread char* threadLocalSendBuf = nullptr;
#endif

void DataSocket::normalFlush()
{
    static  const   int SENDBUF_SIZE = 1024 * 32;
    if (threadLocalSendBuf == nullptr)
    {
        threadLocalSendBuf = (char*)malloc(SENDBUF_SIZE);
    }
    
SEND_PROC:
    char* sendptr = threadLocalSendBuf;
    size_t     wait_send_size = 0;

    for (auto it = mSendList.begin(); it != mSendList.end(); ++it)
    {
        auto& packet = *it;
        auto packetLeftBuf = (char*)(packet.data->c_str() + (packet.data->size() - packet.left));
        auto packetLeftLen = packet.left;

        if ((wait_send_size + packetLeftLen) <= SENDBUF_SIZE)
        {
            memcpy(sendptr + wait_send_size, packetLeftBuf, packetLeftLen);
            wait_send_size += packetLeftLen;
        }
        else
        {
            if (it == mSendList.begin())
            {
                sendptr = packetLeftBuf;
                wait_send_size = packetLeftLen;
            }
            break;
        }
    }

    if (wait_send_size <= 0)
    {
        return;
    }
    
    bool must_close = false;

    int send_retlen = 0;
#ifdef USE_OPENSSL
    if (mSSL != nullptr)
    {
        send_retlen = SSL_write(mSSL, sendptr, wait_send_size);
    }
    else
    {
        send_retlen = ::send(mFD, sendptr, wait_send_size, 0);
    }
#else
    send_retlen = ::send(mFD, sendptr, static_cast<int>(wait_send_size), 0);
#endif

    if (send_retlen > 0)
    {
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
                (*packet.mCompleteCallback)();
            }
            it = mSendList.erase(it);
        }

        if (send_retlen == wait_send_size)
        {
            if (!mSendList.empty())
            {
                goto SEND_PROC;
            }
        }
        else
        {
            mCanWrite = false;
            must_close = !checkWrite();
        }
    }
    else
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
    }

    if (must_close)
    {
        procCloseInLoop();
    }
}

void DataSocket::quickFlush()
{
#ifdef PLATFORM_LINUX
#ifndef MAX_IOVEC
#define  MAX_IOVEC 1024
#endif

    struct iovec iov[MAX_IOVEC];
SEND_PROC:
    bool must_close = false;
    int num = 0;
    int ready_send_len = 0;
    for (PACKET_LIST_TYPE::iterator it = mSendList.begin(); it != mSendList.end();)
    {
        pending_packet& b = *it;
        iov[num].iov_base = (void*)(b.data->c_str() + (b.data->size() - b.left));
        iov[num].iov_len = b.left;
        ready_send_len += b.left;

        ++it;
        num++;
        if (num >=  MAX_IOVEC)
        {
            break;
        }
    }

    if (num == 0)
    {
        return;
    }

    int send_len = writev(mFD, iov, num);
    if (send_len > 0)
    {
        int tmp_len = send_len;
        for (PACKET_LIST_TYPE::iterator it = mSendList.begin(); it != mSendList.end();)
        {
            pending_packet& b = *it;
            if (b.left > tmp_len)
            {
                b.left -= tmp_len;
                break;
            }

            tmp_len -= b.left;
            if (b.mCompleteCallback != nullptr)
            {
                (*b.mCompleteCallback)();
            }
            it = mSendList.erase(it);
        }

        if (send_len == ready_send_len)
        {
            if (!mSendList.empty())
            {
                goto SEND_PROC;
            }
        }
        else
        {
            mCanWrite = false;
            must_close = !checkWrite();
        }
    }
    else
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
    }

    if (must_close)
    {
        procCloseInLoop();
    }
#endif
}

void DataSocket::procCloseInLoop()
{
    if (mFD == SOCKET_ERROR)
    {
        return;
    }

#ifdef PLATFORM_WINDOWS
    if (mPostWriteCheck || mPostRecvCheck)
    {
        closeSocket();
    }
    else
    {
        onClose();
    }
#else
    onClose();
#endif
}

void DataSocket::procShutdownInLoop()
{
    if (mFD != SOCKET_ERROR)
    {
#ifdef PLATFORM_WINDOWS
        shutdown(mFD, SD_SEND);
#else
        shutdown(mFD, SHUT_WR);
#endif
        mCanWrite = false;
    }
}

void DataSocket::onClose()
{
    if (mIsPostFinalClose)
    {
        return;
    }

    mEventLoop->pushAfterLoopProc([=]() {
        if (mDisConnectCallback != nullptr)
        {
            mDisConnectCallback(this);
        }
    });

    closeSocket();
    mIsPostFinalClose = true;
}

void DataSocket::closeSocket()
{
    if (mFD != SOCKET_ERROR)
    {
        mCanWrite = false;
        ox_socket_close(mFD);
        mFD = SOCKET_ERROR;
    }
}

bool    DataSocket::checkRead()
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
    int ret = WSARecv(mFD, &in_buf, 1, &dwBytes, &flag, &(mOvlRecv.base), 0);
    if (ret == -1)
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

bool    DataSocket::checkWrite()
{
    bool check_ret = true;
#ifdef PLATFORM_WINDOWS
    static WSABUF wsendbuf[1] = { {NULL, 0} };

    if (mPostWriteCheck)
    {
        return check_ret;
    }

    DWORD send_len = 0;
    int ret = WSASend(mFD, wsendbuf, 1, &send_len, 0, &(mOvlSend.base), 0);
    if (ret == -1)
    {
        check_ret = (sErrno == WSA_IO_PENDING);
    }

    if (check_ret)
    {
        mPostWriteCheck = true;
    }
#else
    struct epoll_event ev = { 0, { 0 } };
    ev.events = EPOLLET | EPOLLIN | EPOLLOUT | EPOLLRDHUP;
    ev.data.ptr = (Channel*)(this);
    epoll_ctl(mEventLoop->getEpollHandle(), EPOLL_CTL_MOD, mFD, &ev);
#endif

    return check_ret;
}

#ifdef PLATFORM_LINUX
void    DataSocket::removeCheckWrite()
{
    if(mFD != SOCKET_ERROR)
    {
        struct epoll_event ev = { 0, { 0 } };
        ev.events = EPOLLET | EPOLLIN | EPOLLRDHUP;
        ev.data.ptr = (Channel*)(this);
        epoll_ctl(mEventLoop->getEpollHandle(), EPOLL_CTL_MOD, mFD, &ev);
    }
}
#endif

void DataSocket::setEnterCallback(ENTER_CALLBACK cb)
{
    mEnterCallback = std::move(cb);
}

void DataSocket::setDataCallback(DATA_CALLBACK cb)
{
    mDataCallback = std::move(cb);
}

void DataSocket::setDisConnectCallback(DISCONNECT_CALLBACK cb)
{
    mDisConnectCallback = std::move(cb);
}

const static size_t GROW_BUFFER_SIZE = 1024;

void DataSocket::growRecvBuffer()
{
    if (mRecvBuffer == nullptr)
    {
        mRecvBuffer = ox_buffer_new(16*1024+GROW_BUFFER_SIZE);
    }
    else if ((ox_buffer_getsize(mRecvBuffer) + GROW_BUFFER_SIZE) <= mMaxRecvBufferSize)
    {
        buffer_s* newBuffer = ox_buffer_new(ox_buffer_getsize(mRecvBuffer) + GROW_BUFFER_SIZE);
        ox_buffer_write(newBuffer, ox_buffer_getreadptr(mRecvBuffer), ox_buffer_getreadvalidcount(mRecvBuffer));
        ox_buffer_delete(mRecvBuffer);
        mRecvBuffer = newBuffer;
    }
}
void DataSocket::PingCheck()
{
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

void DataSocket::startPingCheckTimer()
{
    if (!mTimer.lock() && mCheckTime > 0)
    {
        mTimer = mEventLoop->getTimerMgr()->addTimer(mCheckTime, [=](){
            PingCheck();
        });
    }
}

void DataSocket::setCheckTime(int overtime)
{
    assert(mEventLoop->isInLoopThread());
    if (!mEventLoop->isInLoopThread())
    {
        return;
    }

    if (overtime > 0)
    {
        mCheckTime = overtime;
        startPingCheckTimer();
    }
    else if (overtime == -1)
    {
        if (mTimer.lock())
        {
            mTimer.lock()->cancel();
        }

        mCheckTime = -1;
    }
}

void DataSocket::postDisConnect()
{
    if (mEventLoop != nullptr)
    {
        mEventLoop->pushAsyncProc([=](){
            procCloseInLoop();
        });
    }
}

void DataSocket::postShutdown()
{
    if (mEventLoop == nullptr)
    {
        return;
    }

    mEventLoop->pushAsyncProc([=]() {
        if (mFD != SOCKET_ERROR)
        {
            mEventLoop->pushAfterLoopProc([=]() {
                procShutdownInLoop();
            });
        }
    });
}

void DataSocket::setUD(BrynetAny value)
{
    mUD = std::move(value);
}

const BrynetAny& DataSocket::getUD() const
{
    return mUD;
}

#ifdef USE_OPENSSL
bool DataSocket::initAcceptSSL(SSL_CTX* ctx)
{
    bool ret = false;

    if(mSSL == nullptr)
    {
        mSSL = SSL_new(ctx);
        ret = SSL_set_fd(mSSL, mFD) == 1;

        if (!ret)
        {
            ERR_print_errors_fp(stdout);
            ::fflush(stdout);
        }
    }

    return ret;
}

bool DataSocket::initConnectSSL()
{
    bool ret = false;

    if(mSSLCtx == nullptr)
    {
        mSSLCtx = SSL_CTX_new(SSLv23_client_method());
        mSSL = SSL_new(mSSLCtx);

        ret = SSL_set_fd(mSSL, mFD) == 1;

        if (!ret)
        {
            ERR_print_errors_fp(stdout);
            ::fflush(stdout);
        }
    }

    return ret;
}

void DataSocket::processSSLHandshake()
{
    if (mIsHandsharked)
    {
        return;
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
            if (mEnterCallback != nullptr)
            {
                mEnterCallback(this);
                mEnterCallback = nullptr;
            }
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
        if (mEnterCallback != nullptr)
        {
            mEnterCallback(this);
            mEnterCallback = nullptr;
        }
        procCloseInLoop();
    }
}
#endif

DataSocket::PACKET_PTR DataSocket::makePacket(const char* buffer, size_t len)
{
    return std::make_shared<std::string>(buffer, len);
}
