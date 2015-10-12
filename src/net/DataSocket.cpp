#include <iostream>
#include <assert.h>
#include <string.h>

#include "SocketLibFunction.h"
#include "EventLoop.h"
#include "buffer.h"

#include "DataSocket.h"

using namespace std;

static int sDefaultRecvBufferSize = 1024;

DataSocket::DataSocket(int fd)
{
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
    memset(&mOvlClose, 0, sizeof(mOvlClose));

    mOvlRecv.OP = EventLoop::OVL_RECV;
    mOvlSend.OP = EventLoop::OVL_SEND;
    mOvlClose.OP = EventLoop::OVL_CLOSE;

    mPostRecvCheck = false;
    mPostWriteCheck = false;
#endif

    mEnterCallback = nullptr;
    mDisConnectCallback = nullptr;
    mDataCallback = nullptr;
    mUserData = -1;

    mRecvBuffer = ox_buffer_new(sDefaultRecvBufferSize);

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
            mEnterCallback(this);
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
void    DataSocket::send(const char* buffer, int len)
{
    sendPacket(makePacket(buffer, len));
}

void DataSocket::sendPacket(const PACKET_PTR& packet)
{
    mEventLoop->pushAsyncProc([this, packet](){
        mSendList.push_back({ packet, packet->size() });
        runAfterFlush();
    });
}

void DataSocket::sendPacket(PACKET_PTR&& packet)
{
    sendPacket(packet);
}

void    DataSocket::canRecv()
{
#ifdef PLATFORM_WINDOWS
    mPostRecvCheck = false;
#endif
    recv();
}

void DataSocket::canSend()
{
#ifdef PLATFORM_WINDOWS
    mPostWriteCheck = false;
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
    static  const   int RECVBUFFER_SIZE = 1024 * 16;

    bool must_close = false;
    static_assert(sizeof(char) == 1, "");
    char tmpRecvBuffer[RECVBUFFER_SIZE];    /*  临时读缓冲区  */
    const char* recvEndPos = tmpRecvBuffer + sizeof(tmpRecvBuffer); /*缓冲区结束位置*/
    char* writePos = tmpRecvBuffer;         /*  当前写起始点  */
    char* parsePos = tmpRecvBuffer;         /*  当前解析消息包起始点  */

    {
        /*  先把内置缓冲区的数据拷贝到读缓冲区,然后置空内置缓冲区 */
        const int oldDataLen = ox_buffer_getreadvalidcount(mRecvBuffer);
        if (sizeof(tmpRecvBuffer) >= oldDataLen && oldDataLen > 0)
        {
            memcpy(tmpRecvBuffer, ox_buffer_getreadptr(mRecvBuffer), oldDataLen);
            writePos += oldDataLen;
        }

        ox_buffer_addreadpos(mRecvBuffer, oldDataLen);
        ox_buffer_adjustto_head(mRecvBuffer);

        assert(ox_buffer_getreadpos(mRecvBuffer) == 0);
        assert(ox_buffer_getwritepos(mRecvBuffer) == 0);
    }

    while (mFD != SOCKET_ERROR)
    {
        const int tryRecvLen = recvEndPos - writePos;
        if (tryRecvLen <= 0)
        {
            break;
        }

        int retlen = 0;
#ifdef USE_OPENSSL
        if (mSSL != nullptr)
        {
            retlen = SSL_read(mSSL, writePoss, tryRecvLen);
        }
        else
        {
            retlen = ::recv(mFD, writePos, tryRecvLen, 0);
        }
#else
        retlen = ::recv(mFD, writePos, tryRecvLen, 0);
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
            if (mDataCallback != nullptr)
            {
                mRecvData = true;
                writePos += retlen;
                int proclen = mDataCallback(this, parsePos, writePos - parsePos);
                assert(proclen >= 0 && proclen <= (writePos - parsePos));
                if (proclen >= 0 && proclen <= (writePos - parsePos))
                {
                    parsePos += proclen;

                    if (writePos == recvEndPos)
                    {
                        const int validLen = writePos - parsePos;
                        if (validLen > 0)
                        {
                            memmove(tmpRecvBuffer, parsePos, validLen);
                        }

                        writePos = tmpRecvBuffer + validLen;
                        parsePos = tmpRecvBuffer;
                    }
                }
                else
                {
                    break;
                }
            }
        }
    }

    /*剩余未解析的数据大小*/
    const int leftlen = writePos - parsePos;
    if (leftlen > 0)
    {
        if ((leftlen > ox_buffer_getwritevalidcount(mRecvBuffer)) && (leftlen + ox_buffer_getreadvalidcount(mRecvBuffer)) <= RECVBUFFER_SIZE)
        {
            /*如果此次需要拷贝的数据大于剩余内置缓冲区大小，（且新分配大小在16k以内,避免而已客户端发送错误packet，导致服务器分配超大内存)则重新分配内置缓冲区*/
            struct buffer_s* newbuffer = ox_buffer_new(leftlen + ox_buffer_getreadvalidcount(mRecvBuffer));
            ox_buffer_write(newbuffer, ox_buffer_getreadptr(mRecvBuffer), ox_buffer_getreadvalidcount(mRecvBuffer));
            ox_buffer_delete(mRecvBuffer);
            mRecvBuffer = newbuffer;
        }

        ox_buffer_write(mRecvBuffer, parsePos, leftlen);
    }

    if (must_close)
    {
        /*  回调到用户层  */
        procCloseInLoop();
    }
    
    assert(leftlen >= 0);
    assert(parsePos <= recvEndPos);
    assert(writePos <= recvEndPos);
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

    static  const   int SENDBUF_SIZE = 1024 * 16;
    char    sendbuf[SENDBUF_SIZE];
    int     wait_send_size = 0;

SEND_PROC:

    wait_send_size = 0;

    for (PACKET_LIST_TYPE::iterator it = mSendList.begin(); it != mSendList.end();)
    {
        pending_packet& b = *it;
        if ((wait_send_size + b.left) <= sizeof(sendbuf))
        {
            memcpy(sendbuf + wait_send_size, b.packet->c_str() + (b.packet->size() - b.left), b.left);
            wait_send_size += b.left;
            ++it;
        }
        else
        {
            break;
        }
    }

    if (wait_send_size > 0)
    {
        int send_retlen = 0;
#ifdef USE_OPENSSL
        if (mSSL != nullptr)
        {
            send_retlen = SSL_write(mSSL, sendbuf, wait_send_size);
        }
        else
        {
            send_retlen = ::send(mFD, sendbuf, wait_send_size, 0);
        }
#else
        send_retlen = ::send(mFD, sendbuf, wait_send_size, 0);
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
            PostQueuedCompletionStatus(mEventLoop->getIOCPHandle(), 0, (ULONG_PTR)((Channel*)this), &(mOvlClose.base));
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
            if (check_ret)
            {
                mPostRecvCheck = true;
            }
        }
        else
        {
            check_ret = true;
            mEventLoop->pushAfterLoopProc([=](){
                recv();
            });
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
            if (check_ret)
            {
                mPostWriteCheck = true;
            }
        }
        else
        {
            check_ret = true;
            mCanWrite = true;
            runAfterFlush();
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
