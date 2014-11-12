#include <iostream>
using namespace std;

#include "eventloop.h"
#include "httprequest.h"
#include "datasocket.h"

DataSocket::DataSocket(int fd)
{
    mEventLoop = nullptr;
    mIsPostFlush = false;
    mFD = fd;

    unsigned long ul = true;

    ioctlsocket(fd, FIONBIO, &ul);

    /*  默认可写    */
    mCanWrite = true;

    memset(&mOvlRecv, sizeof(mOvlRecv), 0);
    memset(&mOvlSend, sizeof(mOvlSend), 0);

    mOvlRecv.base.Offset = OVL_RECV;
    mOvlSend.base.Offset = OVL_SEND;

    mDisConnectHandle = nullptr;
    mDataHandle = nullptr;
}

DataSocket::~DataSocket()
{
    disConnect();
}

void DataSocket::setEventLoop(EventLoop* el)
{
    mEventLoop = el;
    checkRead();
}

/*  添加发送数据队列    */
void    DataSocket::send(const char* buffer, int len)
{
    mEventLoop->pushAsyncProc([this, buffer,len](){
        send_msg* m = new send_msg{ 0, buffer };
        mSendList.push_back({ m, len, len });

        if (!mIsPostFlush)
        {
            mEventLoop->pushAfterLoopProc([this](){
                mIsPostFlush = false;
                flush();
            });

            mIsPostFlush = true;
        }
    });
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
            if (GetLastError() == WSAEWOULDBLOCK)
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
}

void DataSocket::flush()
{
    if (mCanWrite)
    {
        /*  TODO::windows下没有进行消息包合并，而是每一个sendmsg都进行一次send   */
        /*  Linux下使用writev进行合并操作    */
        bool must_close = false;

        for (std::deque<pending_buffer>::iterator it = mSendList.begin(); it != mSendList.end();)
        {
            pending_buffer& b = *it;
            int ret_len = ::send(mFD, b.data->data, b.leftpos, 0);
            if (ret_len < 0)
            {
                if (GetLastError() != WSAEWOULDBLOCK )
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
            else if (ret_len < b.leftpos)
            {
                b.leftpos -= ret_len;
                mCanWrite = false;
                must_close = !checkWrite();
                break;
            }
            else if (ret_len == b.leftpos)
            {
                delete b.data;
                it = mSendList.erase(it);
            }
        }

        if (must_close)
        {
            onClose();
        }
    }
}

void DataSocket::onClose()
{
    if (mFD != SOCKET_ERROR)
    {
        for (std::deque<pending_buffer>::iterator it = mSendList.begin(); it != mSendList.end();)
        {
            pending_buffer& b = *it;
            delete b.data;
        }

        mSendList.clear();

        mCanWrite = false;

        if (mFD != SOCKET_ERROR)
        {
            closesocket(mFD);
            mFD = SOCKET_ERROR;
        }

        if (mDisConnectHandle != nullptr)
        {
            mEventLoop->pushAfterLoopProc([this](){
                mDisConnectHandle(this);
            });
        }
    }
}

int     DataSocket::getFD()
{
    return mFD;
}

bool    DataSocket::checkRead()
{
    static CHAR temp[] = { 0 };
    static WSABUF  in_buf = { 0, temp };

    bool check_ret = true;

    /*  添加可读检测  */
    
    DWORD   dwBytes = 0;
    DWORD   flag = 0;
    int ret = WSARecv(mFD, &in_buf, 1, &dwBytes, &flag, &(mOvlRecv.base), 0);
    if (ret == -1)
    {
        check_ret = GetLastError() == WSA_IO_PENDING;
    }
    return check_ret;
}

bool    DataSocket::checkWrite()
{
    static WSABUF wsendbuf[1] = { {NULL, 0} };

    bool check_ret = true;

    DWORD send_len = 0;
    int ret = WSASend(mFD, wsendbuf, 1, &send_len, 0, &(mOvlSend.base), 0);
    if (ret == -1)
    {
        check_ret = GetLastError() == WSA_IO_PENDING;
    }

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
    onClose();
}