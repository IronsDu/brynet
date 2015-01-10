#ifndef _DATASOCKET_H
#define _DATASOCKET_H

#include <memory>
#include <functional>
#include <deque>

#include "channel.h"

class EventLoop;

class DataSocket : public Channel
{
public:
    typedef std::function<void(DataSocket*, const char* buffer, int len)>    DATA_HANDLE;
    typedef std::function<void(DataSocket*)>                                 DISCONNECT_HANDLE;
    typedef std::shared_ptr<std::string>                                     PACKET_PTR;

public:
    DataSocket(int fd);
    ~DataSocket();

    void                            send(const char* buffer, int len);

    void                            sendPacket(const PACKET_PTR&);
    void                            sendPacket(PACKET_PTR&);
    void                            sendPacket(PACKET_PTR&&);

    void                            setDataHandle(DATA_HANDLE proc);
    void                            setDisConnectHandle(DISCONNECT_HANDLE proc);

    /*主动(请求)断开连接*/
    void                            postDisConnect();

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

    /*此函数主要为了在windows下尝试调用onClose*/
    void                            tryOnClose();
    void                            onClose();
    void                            _procCloseSocket();

    void                            runAfterFlush();

    void                            freeSendPacketList();
private:

#ifdef PLATFORM_WINDOWS
    #include <windows.h>
    struct ovl_ext_s
    {
        OVERLAPPED  base;
    };
    struct ovl_ext_s                mOvlRecv;
    struct ovl_ext_s                mOvlSend;
    struct ovl_ext_s                mOvlClose;

    bool                            mPostRecvCheck;
    bool                            mPostWriteCheck;
    bool                            mPostClose;
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

    DATA_HANDLE                     mDataHandle;
    DISCONNECT_HANDLE               mDisConnectHandle;

    bool                            mIsPostFlush;

    int64_t                         mUserData;
};

#endif