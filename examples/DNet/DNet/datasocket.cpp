#include <iostream>
using namespace std;

#include "platform.h"
#ifdef PLATFORM_WINDOWS
#else
#include <sys/uio.h>
#endif

#include "eventloop.h"
#include "socketlibtypes.h"
#include "datasocket.h"

DataSocket::DataSocket(int fd)
{
    mEventLoop = nullptr;
    mIsPostFlush = false;
    mFD = fd;
    /*  默认可写    */
    mCanWrite = true;

    unsigned long ul = true;

#ifdef PLATFORM_WINDOWS
    ioctlsocket(fd, FIONBIO, &ul);
    memset(&mOvlRecv, sizeof(mOvlRecv), 0);
    memset(&mOvlSend, sizeof(mOvlSend), 0);

    mOvlRecv.base.Offset = OVL_RECV;
    mOvlSend.base.Offset = OVL_SEND;
#else
    ioctl(fd, FIONBIO, &ul);
#endif

    mDisConnectHandle = nullptr;
    mDataHandle = nullptr;
    mUserData = -1;
}

DataSocket::~DataSocket()
{
    disConnect();
}

void DataSocket::setEventLoop(EventLoop* el)
{
    if (mEventLoop == nullptr)
    {
        mEventLoop = el;
        checkRead();
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
    bool must_close = false;
    char temp[1024*10];
    while (true)
    {
        int retlen = recv(mFD, temp, 1024 * 10, 0);
        if (retlen < 0)
        {
            if (sErrno == S_EWOULDBLOCK)
            {
                must_close = !checkRead();
            }
            else
            {
                must_close = true;
            }

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
                mDataHandle(this, temp, retlen);
            }
        }
    }

    if (must_close)
    {
        /*  回调到用户层  */
        onClose();
    }

    return;
}

void DataSocket::canSend()
{
    mCanWrite = true;
    runAfterFlush();
}

void DataSocket::runAfterFlush()
{
    if (!mIsPostFlush)
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

void DataSocket::flush()
{
    if (mCanWrite)
    {
        /*  TODO::windows下没有进行消息包合并，而是每一个sendmsg都进行一次send   */
        /*  Linux下使用writev进行合并操作    */
#ifdef PLATFORM_WINDOWS
        bool must_close = false;

        for (PACKET_LIST_TYPE::iterator it = mSendList.begin(); it != mSendList.end();)
        {
            pending_packet& b = *it;
            int ret_len = ::send(mFD, b.packet->c_str() + (b.packet->size() - b.left), b.left, 0);
            if (ret_len < 0)
            {
                if (sErrno != S_EWOULDBLOCK)
                {
                    must_close = true;
                }
                else
                {
                    mCanWrite = false;
                    must_close = !checkWrite();
                }

                break;
            }
            else if (ret_len == 0)
            {
                must_close = true;
                break;
                /*  close */
            }
            else if (ret_len < b.left)
            {
                b.left -= ret_len;
                mCanWrite = false;
                must_close = !checkWrite();
                break;
            }
            else if (ret_len == b.left)
            {
                it = mSendList.erase(it);
            }
        }

        if (must_close)
        {
            onClose();
        }
#else
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

            num++;
            if(num <= MAX_IOVEC)
            {
                break;
            }
        }

        if (num > 0)
        {
            int send_len = writev(mFD, iov, num);
            if(send_len > 0)
            {
                int total_len = send_len;
                for (PACKET_LIST_TYPE::iterator it = mSendList.begin(); it != mSendList.end();)
                {
                    pending_packet& b = *it;
                    if (b.left <= total_len)
                    {
                        total_len -= b.left;
                        it = mSendList.erase(it);
                    }
                    else
                    {
                        b.left -= total_len;
                        break;
                    }
                }

                if(!mSendList.empty())
                {
                    goto SEND_PROC;
                }
            }
            else
            {
                if (sErrno == S_EWOULDBLOCK)
                {
                    must_close = true;
                }
            }
        }

        if (must_close)
        {
            onClose();
        }
#endif
    }
}

void DataSocket::onClose()
{
    if (mFD != SOCKET_ERROR)
    {
        _procClose();

        if (mDisConnectHandle != nullptr)
        {
            mEventLoop->pushAfterLoopProc([this](){
                mDisConnectHandle(this);
            });
        }
    }
}

void DataSocket::_procClose()
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

    DWORD   dwBytes = 0;
    DWORD   flag = 0;
    int ret = WSARecv(mFD, &in_buf, 1, &dwBytes, &flag, &(mOvlRecv.base), 0);
    if (ret == -1)
    {
        check_ret = GetLastError() == WSA_IO_PENDING;
    }
#else
#endif
    
    return check_ret;
}

bool    DataSocket::checkWrite()
{
    bool check_ret = true;
#ifdef PLATFORM_WINDOWS
    static WSABUF wsendbuf[1] = { {NULL, 0} };

    DWORD send_len = 0;
    int ret = WSASend(mFD, wsendbuf, 1, &send_len, 0, &(mOvlSend.base), 0);
    if (ret == -1)
    {
        check_ret = GetLastError() == WSA_IO_PENDING;
    }
#else
#endif

    return check_ret;
}

void DataSocket::setDataHandle(DATA_PROC proc)
{
    mDataHandle = proc;
}

void DataSocket::setDisConnectHandle(DISCONNECT_PROC proc)
{
    mDisConnectHandle = proc;
}

void DataSocket::disConnect()
{
    _procClose();
}

void DataSocket::setUserData(int64_t value)
{
    mUserData = value;
}

int64_t DataSocket::getUserData() const
{
    return mUserData;
}

DataSocket::PACKET_PTR DataSocket::makePacket(const char* buffer, int len)
{
    return std::make_shared<string>(buffer, len);
}