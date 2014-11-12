#ifndef _TCP_SERVER_H
#define _TCP_SERVER_H

#include "eventloop.h"
#include "datasocket.h"

class TcpServer
{
public:
    TcpServer(int port, int threadNum);
    ~TcpServer();

    void                                setEnterHandle(EventLoop::CONNECTION_ENTER_HANDLE handle);
    void                                setDisconnectHandle(DataSocket::DISCONNECT_PROC handle);
    void                                setMsgHandle(DataSocket::DATA_PROC handle);

private:
    void                                RunListen(int port);

private:
    EventLoop*                          mLoops;
    std::thread**                       mIOThreads;
    int                                 mLoopNum;
    std::thread*                        mListenThread;

    EventLoop::CONNECTION_ENTER_HANDLE  mEnterHandle;
    DataSocket::DISCONNECT_PROC         mDisConnectHandle;
    DataSocket::DATA_PROC               mDataProc;
};

#endif