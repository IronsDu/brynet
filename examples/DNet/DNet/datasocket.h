#ifndef _DATASOCKET_H
#define _DATASOCKET_H

#include "eventloop.h"
#include "channel.h"

#include <memory>
#include <functional>
#include <deque>
using namespace std;

class DataSocket : public Channel
{
public:
    typedef function<void(DataSocket*, const char* buffer, int len)>    DATA_PROC;
    typedef function<void(DataSocket*)>                                 DISCONNECT_PROC;
    typedef std::shared_ptr<string>                                     PACKET_PTR;

public:
    DataSocket(int fd);
    ~DataSocket();

                                    /*  添加发送数据队列    */
    void                            send(const char* buffer, int len);

    void                            sendPacket(const PACKET_PTR&);
    void                            sendPacket(PACKET_PTR&);
    void                            sendPacket(PACKET_PTR&&);
    void                            setDataHandle(DATA_PROC proc);
    void                            setDisConnectHandle(DISCONNECT_PROC proc);

    /*主动断开此链接，（则）不会触发断开回调*/
    void                            disConnect();

    void                            setUserData(int64_t value);
    int64_t                         getUserData() const;

    static  PACKET_PTR              makePacket(const char* buffer, int len);
private:
    void                            setEventLoop(EventLoop* el);

    void                            canRecv();
    void                            canSend();

    bool                            checkRead();
    bool                            checkWrite();

    void                            flush();
    void                            onClose();
    void                            _procClose();

    void                            runAfterFlush();

    void                            freeSendPacketList();
private:
#ifdef PLATFORM_WINDOWS
    struct ovl_ext_s
    {
        OVERLAPPED  base;
    };
    struct ovl_ext_s                mOvlRecv;
    struct ovl_ext_s                mOvlSend;
#else

#endif

    int                             mFD;
    EventLoop*                      mEventLoop;

    struct pending_packet
    {
        PACKET_PTR  packet;
        size_t      left;
    };

    typedef std::deque<pending_packet>   PACKET_LIST_TYPE;
    PACKET_LIST_TYPE                mSendList;

    bool                            mCanWrite;

    DATA_PROC                       mDataHandle;
    DISCONNECT_PROC                 mDisConnectHandle;

    bool                            mIsPostFlush;

    int64_t                         mUserData;
};

#endif