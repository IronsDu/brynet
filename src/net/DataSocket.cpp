#include <iostream>
#include <assert.h>
#include <string.h>

#include "SocketLibFunction.h"
#include "EventLoop.h"
#include "buffer.h"

#include "DataSocket.h"

using namespace std;


DataSocket::DataSocket(int fd, int maxRecvBufferSize)
{
    mMaxRecvBufferSize = maxRecvBufferSize;
    mRecvData = false;
    mCheckTime = -1;
    mIsPostFinalClose = false;
    mEventLoop = nullptr;
    mIsPostFlush = false;
    mFD = fd;

    /*  默认可写    */
    mCanWrite = true;

#ifdef PLATFORM_WINDOWS
    memset(&mOvlRecv, 0, sizeof(mOvlRecv));
    memset(&mOvlSend, 0, sizeof(mOvlSend));

    mOvlRecv.OP = EventLoop::OVL_RECV;
    mOvlSend.OP = EventLoop::OVL_SEND;

    mPostRecvCheck = false;
    mPostWriteCheck = false;
#endif

    mEnterCallback = nullptr;
    mDisConnectCallback = nullptr;
    mDataCallback = nullptr;
    mUserData = -1;
    mRecvBuffer = nullptr;
    growRecvBuffer();

#ifdef USE_OPENSSL
    mSSLCtx = nullptr;
    mSSL = nullptr;
#endif
}

DataSocket::~DataSocket()
{
    assert(mFD == SOCKET_ERROR);

    if (mRecvBuffer != nullptr)
    {
        ox_buffer_delete(mRecvBuffer);
        mRecvBuffer = nullptr;
    }

#ifdef USE_OPENSSL
    if (mSSLCtx != nullptr)
    {
        SSL_CTX_free(mSSLCtx);
        mSSLCtx = nullptr;
    }
    if (mSSL != nullptr)
    {
        SSL_free(mSSL);
        mSSL = nullptr;
    }
#endif

    closeSocket();

    if (mTimer.lock())
    {
        mTimer.lock()->Cancel();
    }
}

bool DataSocket::onEnterEventLoop(EventLoop* el)
{
    bool ret = false;

    if (mEventLoop == nullptr)
    {
        mEventLoop = el;

        ox_socket_nonblock(mFD);
        mEventLoop->linkChannel(mFD, this);
        
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

/*  添加发送数据队列    */
void    DataSocket::send(const char* buffer, int len, const PACKED_SENDED_CALLBACK& callback)
{
    sendPacket(makePacket(buffer, len), callback);
}

void DataSocket::sendPacketInLoop(const PACKET_PTR& packet, const PACKED_SENDED_CALLBACK& callback)
{
    assert(mEventLoop->isInLoopThread());
    mSendList.push_back({ packet, packet->size(), callback });
    runAfterFlush();
}

void DataSocket::sendPacketInLoop(PACKET_PTR&& packet, const PACKED_SENDED_CALLBACK& callback)
{
    assert(mEventLoop->isInLoopThread());
    sendPacketInLoop(packet, callback);
}

void DataSocket::sendPacket(const PACKET_PTR& packet, const PACKED_SENDED_CALLBACK& callback)
{
    mEventLoop->pushAsyncProc([this, packet, callback](){
        mSendList.push_back({ packet, packet->size(), callback });
        runAfterFlush();
    });
}

void DataSocket::sendPacket(PACKET_PTR&& packet, const PACKED_SENDED_CALLBACK& callback)
{
    sendPacket(packet, callback);
}

void    DataSocket::canRecv()
{
#ifdef PLATFORM_WINDOWS
    mPostRecvCheck = false;
    if (mFD == SOCKET_ERROR && !mPostWriteCheck && !mPostRecvCheck)
    {
        onClose();
        return;
    }
#endif
    recv();
}

void DataSocket::canSend()
{
#ifdef PLATFORM_WINDOWS
    mPostWriteCheck = false;
    if (mFD == SOCKET_ERROR && !mPostWriteCheck && !mPostRecvCheck)
    {
        onClose();
        return;
    }
#else
    removeCheckWrite();
#endif
    mCanWrite = true;
    runAfterFlush();
}

void DataSocket::runAfterFlush()
{
    if (!mIsPostFlush && !mSendList.empty() && mFD != SOCKET_ERROR)
    {
        mEventLoop->pushAfterLoopProc([this](){
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
        const int tryRecvLen = ox_buffer_getwritevalidcount(mRecvBuffer);
        if (tryRecvLen <= 0)
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
        retlen = ::recv(mFD, ox_buffer_getwriteptr(mRecvBuffer), tryRecvLen, 0);
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

            if (mDataCallback != nullptr)
            {
                mRecvData = true;
                int proclen = mDataCallback(this, ox_buffer_getreadptr(mRecvBuffer), ox_buffer_getreadvalidcount(mRecvBuffer));
                assert(proclen >= 0 && proclen <= ox_buffer_getreadvalidcount(mRecvBuffer));
                if (proclen >= 0 && proclen <= ox_buffer_getreadvalidcount(mRecvBuffer))
                {
                    ox_buffer_addreadpos(mRecvBuffer, proclen);
                }
                else
                {
                    break;
                }
            }
        }

        if (ox_buffer_getwritevalidcount(mRecvBuffer) == 0)
        {
            ox_buffer_adjustto_head(mRecvBuffer);
        }

        if (ox_buffer_getwritevalidcount(mRecvBuffer) == 0 || retlen == ox_buffer_getsize(mRecvBuffer))
        {
            growRecvBuffer();
        }
    }

    if (must_close)
    {
        /*  回调到用户层  */
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

void DataSocket::normalFlush()
{
    bool must_close = false;

    static  const   int SENDBUF_SIZE = 1024 * 32;
    char    sendbuf[SENDBUF_SIZE];
    /*TODO::将sendbuf改为堆内存或线程私有数据， 提高此内存区域大小，提高吞吐*/
    
SEND_PROC:
    char* sendptr = sendbuf;
    int     wait_send_size = 0;

    for (PACKET_LIST_TYPE::iterator it = mSendList.begin(); it != mSendList.end(); ++it)
    {
        pending_packet& b = *it;
        char* packetLeftBuf = (char*)(b.packet->c_str() + (b.packet->size() - b.left));
        int packetLeftLen = b.left;

        if ((wait_send_size + packetLeftLen) <= sizeof(sendbuf))
        {
            memcpy(sendptr + wait_send_size, packetLeftBuf, packetLeftLen);
            wait_send_size += packetLeftLen;
        }
        else
        {
            /*如果无法装入合并缓冲区，则单独发送此消息包*/
            if (it == mSendList.begin())
            {
                sendptr = packetLeftBuf;
                wait_send_size = packetLeftLen;
            }
            break;
        }
    }

    if (wait_send_size > 0)
    {
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
        send_retlen = ::send(mFD, sendptr, wait_send_size, 0);
#endif

        if (send_retlen > 0)
        {
            size_t tmp_len = send_retlen;
            for (PACKET_LIST_TYPE::iterator it = mSendList.begin(); it != mSendList.end();)
            {
                pending_packet& b = *it;
                if (b.left <= tmp_len)
                {
                    tmp_len -= b.left;
                    if (b.mCompleteCallback != nullptr)
                    {
                        (*b.mCompleteCallback)();
                    }
                    it = mSendList.erase(it);
                }
                else
                {
                    b.left -= tmp_len;
                    break;
                }
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
        iov[num].iov_base = (void*)(b.packet->c_str() + (b.packet->size() - b.left));
        iov[num].iov_len = b.left;
        ready_send_len += b.left;

        ++it;
        num++;
        if (num >=  MAX_IOVEC)
        {
            break;
        }
    }

    if (num > 0)
    {
        int send_len = writev(mFD, iov, num);
        if (send_len > 0)
        {
            int tmp_len = send_len;
            for (PACKET_LIST_TYPE::iterator it = mSendList.begin(); it != mSendList.end();)
            {
                pending_packet& b = *it;
                if (b.left <= tmp_len)
                {
                    tmp_len -= b.left;
                    if(b.mCompleteCallback != nullptr)
                    {
                        (*b.mCompleteCallback)();
                    }
                    it = mSendList.erase(it);
                }
                else
                {
                    b.left -= tmp_len;
                    break;
                }
            }

            if(send_len == ready_send_len)
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
    }

    if (must_close)
    {
        procCloseInLoop();
    }
#endif
}

/*处理读写时得到网络断开(以及强制断开网络)*/
void DataSocket::procCloseInLoop()
{
    if (mFD != SOCKET_ERROR)
    {
#ifdef PLATFORM_WINDOWS
        if (mPostWriteCheck || mPostRecvCheck)  //如果投递了IOCP请求，那么需要投递close，而不能直接调用onClose
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
}

/*当收到网络断开(或者IOCP下收到模拟的断开通知)后的处理(此函数里的主体逻辑只能被执行一次)*/
void DataSocket::onClose()
{
    if (!mIsPostFinalClose)
    {
        DataSocket::PTR thisPtr = this;
        mEventLoop->pushAfterLoopProc([=](){
            if (mDisConnectCallback != nullptr)
            {
                mDisConnectCallback(thisPtr);
            }
        });

        closeSocket();
        mIsPostFinalClose = true;
    }
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

    /*  添加可读检测  */

    if (!mPostRecvCheck)
    {
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
    }
#endif
    
    return check_ret;
}

bool    DataSocket::checkWrite()
{
    bool check_ret = true;
#ifdef PLATFORM_WINDOWS
    static WSABUF wsendbuf[1] = { {NULL, 0} };

    if (!mPostWriteCheck)
    {
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
    mEnterCallback = cb;
}

void DataSocket::setDataCallback(DATA_CALLBACK cb)
{
    mDataCallback = cb;
}

void DataSocket::setDisConnectCallback(DISCONNECT_CALLBACK cb)
{
    mDisConnectCallback = cb;
}

const static int GROW_BUFFER_SIZE = 1024;

void DataSocket::growRecvBuffer()
{
    if (mRecvBuffer == nullptr)
    {
        mRecvBuffer = ox_buffer_new(GROW_BUFFER_SIZE);
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
        mTimer = mEventLoop->getTimerMgr().AddTimer(mCheckTime, [this](){
            this->PingCheck();
        });
    }
}

void DataSocket::setCheckTime(int overtime)
{
    if (overtime > 0)
    {
        mCheckTime = overtime;
        startPingCheckTimer();
    }
    else if (overtime == -1)
    {
        if (mTimer.lock())
        {
            mTimer.lock()->Cancel();
        }

        mCheckTime = -1;
    }
}

void DataSocket::postDisConnect()
{
    if (mEventLoop != nullptr)
    {
        DataSocket::PTR tmp = this;
        mEventLoop->pushAsyncProc([=](){
            tmp->procCloseInLoop();
        });
    }
}

void DataSocket::setUserData(int64_t value)
{
    mUserData = value;
}

int64_t DataSocket::getUserData() const
{
    return mUserData;
}

#ifdef USE_OPENSSL
void DataSocket::setupAcceptSSL(SSL_CTX* ctx)
{
    if(mSSL == nullptr)
    {
        mSSL = SSL_new(ctx);
        SSL_set_fd(mSSL, mFD);
        SSL_accept(mSSL);
    }
}

void DataSocket::setupConnectSSL()
{
    if(mSSLCtx == nullptr)
    {
        mSSLCtx = SSL_CTX_new(SSLv23_client_method());
        mSSL = SSL_new(mSSLCtx);
        SSL_set_fd(mSSL, mFD);
        SSL_connect(mSSL);
    }
}
#endif

DataSocket::PACKET_PTR DataSocket::makePacket(const char* buffer, int len)
{
    return std::make_shared<std::string>(buffer, len);
}
