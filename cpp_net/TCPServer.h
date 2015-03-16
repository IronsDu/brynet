#ifndef _TCP_SERVER_H
#define _TCP_SERVER_H

#include <vector>
#include <string>
#include <functional>
#include <thread>
#include <assert.h>
#include <stdint.h>

#include "datasocket.h"
#include "typeids.h"

class EventLoop;
class DataSocket;

class TcpServer
{
    typedef std::function<void (EventLoop&)> FRAME_CALLBACK;
    typedef std::function<void(int64_t)>    CONNECTION_ENTER_HANDLE;
    typedef std::function<void(int64_t)>    DISCONNECT_HANDLE;
    typedef std::function<int (int64_t, const char* buffer, int len)>    DATA_HANDLE;

public:
    TcpServer();
    ~TcpServer();

    void                                setEnterHandle(TcpServer::CONNECTION_ENTER_HANDLE handle);
    void                                setDisconnectHandle(TcpServer::DISCONNECT_HANDLE handle);
    void                                setMsgHandle(TcpServer::DATA_HANDLE handle);

    void                                send(int64_t id, DataSocket::PACKET_PTR&& packet);
    void                                send(int64_t id, DataSocket::PACKET_PTR& packet);

    /*主动断开此id链接，但仍然会收到此id的断开回调，需要上层逻辑自己处理这个"问题"(尽量统一在断开回调函数里做清理等工作) */
    void                                disConnect(int64_t id);
    
    /*  开启服务    */
    void                                startService(int port, const char *certificate, const char *privatekey, int threadNum, FRAME_CALLBACK callback = nullptr);
    /*  关闭服务    */
    void                                closeService();
    
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

    /*  以下三个回调函数会在多线程中调用(每个线程即一个eventloop驱动的io loop)(见：RunListen中的使用)   */
    TcpServer::CONNECTION_ENTER_HANDLE  mEnterHandle;
    TcpServer::DISCONNECT_HANDLE        mDisConnectHandle;
    TcpServer::DATA_HANDLE              mDataHandle;

    /*  此结构用于标示一个回话，逻辑线程和网络线程通信中通过此结构对回话进行相关操作(而不是直接传递Channel/DataSocket指针)  */
    
    union SessionId
    {
        struct
        {
            uint16_t    loopIndex;      /*  会话所属的eventloop的(在mLoops中的)索引  */
            uint16_t    index;          /*  会话在mIds[loopIndex]中的索引值 */
            uint32_t    iid;            /*  自增计数器   */
        }data;  /*  warn::so,服务器最大支持0xFFFF(65536)个io loop线程，每一个io loop最大支持0xFFFF(65536)个链接。*/

        int64_t id;
    };
    
    std::string                         mCertificate;
    std::string                         mPrivatekey;

    int                                 mPort;
    bool                                mRunService;
};

#endif