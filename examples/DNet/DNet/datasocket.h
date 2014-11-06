#ifndef _DATASOCKET_H
#define _DATASOCKET_H

#include "eventloop.h"
#include "channel.h"

#include <functional>
#include <deque>
using namespace std;

struct ovl_ext_s
{
    OVERLAPPED  base;
    void*   ptr;
};

struct send_msg
{
    int ref;
    const char*   data;
};

struct pending_buffer
{
    send_msg* data;
    int leftpos;
    int maxlen;
};

class DataSocket : public Channel
{
    typedef function<void(DataSocket*, const char* buffer, int len)>    DATA_PROC;
    typedef function<void(DataSocket*)>                                 DISCONNECT_PROC;
public:
    DataSocket(int fd);

    void                            setEventLoop(EventLoop* el);

                                    /*  添加发送数据队列    */
    void                            send(const char* buffer, int len);

    int                             getFD();

    void                            setDataHandle(DATA_PROC proc);
    void                            setDisConnectHandle(DISCONNECT_PROC proc);

    void                            disConnect();

private:
    void                            canRecv();
    void                            canSend();

    bool                            checkRead();
    bool                            checkWrite();

    void                            flush();
    void                            procClose();
private:
    struct ovl_ext_s                mOvlRecv;
    struct ovl_ext_s                mOvlSend;

    int                             mFD;
    EventLoop*                      mEventLoop;
    std::deque<pending_buffer>      mSendList;

    bool                            mCanWrite;

    DATA_PROC                       mDataHandle;
    DISCONNECT_PROC                 mDisConnectHandle;

    bool                            mIsPostFlush;
};


#endif