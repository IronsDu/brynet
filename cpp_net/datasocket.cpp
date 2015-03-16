#include <iostream>

#include "socketlibtypes.h"
#include "eventloop.h"
#include "buffer.h"

#include "datasocket.h"

DataSocket::DataSocket(int fd)
{
    mEventLoop = nullptr;
    mIsPostFlush = false;
    mFD = fd;
    /*  默认可写    */
    mCanWrite = true;

#ifdef PLATFORM_WINDOWS
    memset(&mOvlRecv, sizeof(mOvlRecv), 0);
    memset(&mOvlSend, sizeof(mOvlSend), 0);
    memset(&mOvlClose, sizeof(mOvlClose), 0);

    mOvlRecv.base.Offset = EventLoop::OVL_RECV;
    mOvlSend.base.Offset = EventLoop::OVL_SEND;
    mOvlClose.base.Offset = EventLoop::OVL_CLOSE;

    mPostRecvCheck = false;
    mPostWriteCheck = false;
    mPostClose = false;
#endif

    mDisConnectHandle = nullptr;
    mDataHandle = nullptr;
    mUserData = -1;

    mRecvBuffer = ox_buffer_new(1024);

#ifdef USE_OPENSSL
    mSSLCtx = nullptr;
    mSSL = nullptr;
#endif
}

DataSocket::~DataSocket()
{
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
}
    

void DataSocket::setNoBlock()
{
#ifdef PLATFORM_WINDOWS
    unsigned long ul = true;
    ioctlsocket(mFD, FIONBIO, &ul);
#else
    ioctl(fd, FIONBIO, &ul);
#endif
}

void DataSocket::setEventLoop(EventLoop* el)
{
    if (mEventLoop == nullptr)
    {
        mEventLoop = el;
        if (!checkRead())
        {
            tryOnClose();
        }
    }
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

void DataSocket::sendPacket(PACKET_PTR& packet)
{
    mEventLoop->pushAsyncProc([this, packet](){
        mSendList.push_back({ packet, packet->size()});
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
    mEventLoop->pushAfterLoopProc([this](){
        recv();
    });
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

void DataSocket::freeSendPacketList()
{
    mSendList.clear();
}

void DataSocket::recv()
{
    static  const   int RECVBUFFER_SIZE = 1024 * 16;

    bool must_close = false;
    char tmpRecvBuffer[RECVBUFFER_SIZE];    /*  临时读缓冲区  */
    int recvBufferWritePos = 0;             /*  当前临时读缓冲区的写入位置   */
    int bufferParseStart = 0;               /*  当前消息包解析起始点  */

    {
        /*  先把内置缓冲区的数据拷贝到读缓冲区,然后置空内置缓冲区 */
        memcpy(tmpRecvBuffer, ox_buffer_getreadptr(mRecvBuffer), ox_buffer_getreadvalidcount(mRecvBuffer));
        recvBufferWritePos += ox_buffer_getreadvalidcount(mRecvBuffer);
        ox_buffer_addreadpos(mRecvBuffer, ox_buffer_getreadvalidcount(mRecvBuffer));
        ox_buffer_adjustto_head(mRecvBuffer);
    }

    while (mFD != SOCKET_ERROR)
    {
        int retlen = 0;
#ifdef USE_OPENSSL
        if (mSSL != nullptr)
        {
            retlen = SSL_read(mSSL, tmpRecvBuffer + recvBufferWritePos, sizeof(tmpRecvBuffer)-recvBufferWritePos);
        }
        else
        {
            retlen = ::recv(mFD, tmpRecvBuffer + recvBufferWritePos, sizeof(tmpRecvBuffer)-recvBufferWritePos, 0);
        }
#else
        retlen = ::recv(mFD, tmpRecvBuffer + recvBufferWritePos, sizeof(tmpRecvBuffer)-recvBufferWritePos, 0);
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
            if (mDataHandle != nullptr)
            {
                recvBufferWritePos += retlen;
                int proclen = mDataHandle(this, tmpRecvBuffer + bufferParseStart, recvBufferWritePos-bufferParseStart);
                bufferParseStart += proclen;

                if (recvBufferWritePos == sizeof(tmpRecvBuffer))
                {
                    recvBufferWritePos -= bufferParseStart;
                    bufferParseStart = 0;
                }
            }
        }
    }

    int leftlen = recvBufferWritePos - bufferParseStart;
    if (leftlen > 0)
    {
        /*  表示有剩余未解析数据,需要拷贝到内置缓冲区   */
        if (leftlen > ox_buffer_getsize(mRecvBuffer))
        {
            struct buffer_s* newbuffer = ox_buffer_new(leftlen);
            ox_buffer_delete(mRecvBuffer);
            mRecvBuffer = newbuffer;
        }

        ox_buffer_write(mRecvBuffer, tmpRecvBuffer + bufferParseStart, leftlen);
    }

    if (must_close)
    {
        /*  回调到用户层  */
        tryOnClose();
    }
}

void DataSocket::flush()
{
    if (mCanWrite && mFD != SOCKET_ERROR)
    {
#ifdef PLATFORM_WINDOWS
        normalFlush();
#else
        if (mSSL != nullptr)
        {
            normalFlush();
        }
        else
        {
            quickFlush();
        }
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
        tryOnClose();
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
        if (num <= MAX_IOVEC)
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
        tryOnClose();
    }
#endif
}

void DataSocket::tryOnClose()
{
#ifdef PLATFORM_WINDOWS
    /*  windows下需要没有投递过任何IOCP请求才能真正处理close,否则等待所有通知到达*/
    if (!mPostWriteCheck && !mPostRecvCheck && !mPostClose)
    {
        onClose();
    }
#else
    onClose();
#endif
}

void DataSocket::onClose()
{
    if (mFD != SOCKET_ERROR)
    {
        _procCloseSocket();

        if (mDisConnectHandle != nullptr)
        {
            /*  投递的lambda函数绑定的是一个mDisConnectHandle的拷贝，它的闭包值也会拷贝，避免了lambda执行时删除DataSocket*后则造成
                mDisConnectHandle析构，然后闭包变量就失效的宕机问题  */
            DISCONNECT_HANDLE temp = mDisConnectHandle;
            mEventLoop->pushAfterLoopProc([temp, this](){
                temp(this);
            });
        }
    }
}

void DataSocket::_procCloseSocket()
{
    if (mFD != SOCKET_ERROR)
    {
        freeSendPacketList();

        mCanWrite = false;

        #ifdef PLATFORM_WINDOWS
        closesocket(mFD);
        #else
        close(mFD);
        #endif

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
            check_ret = GetLastError() == WSA_IO_PENDING;
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
            check_ret = GetLastError() == WSA_IO_PENDING;
        }

        if (check_ret)
        {
            mPostWriteCheck = true;
        }
    }
#else
    struct epoll_event ev = { 0, { 0 } };
    ev.events = EPOLLET | EPOLLIN | EPOLLOUT | EPOLLRDHUP;
    ev.data.ptr = ptr;
    epoll_ctl(mEventLoop->getEpollHandle(), EPOLL_CTL_MOD, mFD, &ev);
#endif

    return check_ret;
}

#ifdef PLATFORM_LINUX
void    DataSocket::removeCheckWrite()
{
    struct epoll_event ev = { 0, { 0 } };
    ev.events = EPOLLET | EPOLLIN | EPOLLRDHUP;
    ev.data.ptr = ptr;
    epoll_ctl(mEventLoop->getEpollHandle(), EPOLL_CTL_MOD, mFD, &ev);
}
#endif

void DataSocket::setDataHandle(DATA_HANDLE proc)
{
    mDataHandle = proc;
}

void DataSocket::setDisConnectHandle(DISCONNECT_HANDLE proc)
{
    mDisConnectHandle = proc;
}

void DataSocket::postDisConnect()
{
#ifdef PLATFORM_WINDOWS
    if (mPostWriteCheck || mPostRecvCheck)
    {
        if (!mPostClose)
        {
            _procCloseSocket();
            PostQueuedCompletionStatus(mEventLoop->getIOCPHandle(), 0, (ULONG_PTR)((Channel*)this), &(mOvlClose.base));
            mPostClose = true;
        }
    }
    else
    {
        onClose();
    }
#else
    onClose();
#endif
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
    mSSL = SSL_new(ctx);
    SSL_set_fd(mSSL, mFD);
    SSL_accept(mSSL);
}

void DataSocket::setupConnectSSL()
{
    mSSLCtx = SSL_CTX_new(SSLv23_client_method());
    mSSL = SSL_new(mSSLCtx);
    SSL_set_fd(mSSL, mFD);
    SSL_connect(mSSL);
}
#endif

DataSocket::PACKET_PTR DataSocket::makePacket(const char* buffer, int len)
{
    return std::make_shared<std::string>(buffer, len);
}