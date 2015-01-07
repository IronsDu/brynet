#ifndef _TCP_SERVER_H
#define _TCP_SERVER_H

#include <vector>
#include <assert.h>

#include "typeids.h"

class EventLoop;
class DataSocket;

class TcpServer
{
    typedef std::function<void (EventLoop&)> FRAME_CALLBACK;
    typedef std::function<void(int64_t)>    CONNECTION_ENTER_HANDLE;
    typedef std::function<void(int64_t)>    DISCONNECT_PROC;
    typedef std::function<void(int64_t, const char* buffer, int len)>    DATA_PROC;

public:
    TcpServer(int port, int threadNum, FRAME_CALLBACK callback = nullptr);  /*callback为IO线程每个loop循环都会执行的回调函数，可以为null*/
    ~TcpServer();

    void                                setEnterHandle(TcpServer::CONNECTION_ENTER_HANDLE handle);
    void                                setDisconnectHandle(TcpServer::DISCONNECT_PROC handle);
    void                                setMsgHandle(TcpServer::DATA_PROC handle);

    void                                send(int64_t id, DataSocket::PACKET_PTR&& packet);
    void                                send(int64_t id, DataSocket::PACKET_PTR& packet);

    /*主动断开此id链接，但仍然可能收到此id的断开回调，需要上层逻辑自己处理这个"问题"*/
    void                                disConnect(int64_t id);

private:
    void                                RunListen(int port);
    int64_t                             MakeID(int loopIndex);

    void                                _procDataSocketClose(DataSocket*);

private:
    EventLoop*                          mLoops;
    std::thread**                       mIOThreads;
    int                                 mLoopNum;
    std::thread*                        mListenThread;

    TypeIDS<DataSocket>*                mIds;
    int*                                mIncIds;

    TcpServer::CONNECTION_ENTER_HANDLE  mEnterHandle;
    TcpServer::DISCONNECT_PROC          mDisConnectHandle;
    TcpServer::DATA_PROC                mDataProc;

    union SessionId
    {
        struct
        {
            int8_t loopIndex;
            int32_t index:24;
            int32_t iid;
        }data;  /*TODO::so,服务器最大支持0x7f个io loop线程，每一个io loop最大支持0x7fffff个链接。*/

        int64_t id;
    };
};

#endif