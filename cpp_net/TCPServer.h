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

class ListenThread
{
public:
    typedef std::function<void(int fd)> ACCEPT_CALLBACK;
    ListenThread();
    ~ListenThread();

    /*  开启监听线程  */
    void                                startListen(int port, const char *certificate, const char *privatekey, ACCEPT_CALLBACK callback);
    void                                closeListenThread();
#ifdef USE_OPENSSL
    SSL_CTX*                            getOpenSSLCTX();
#endif
private:
    void                                RunListen();
    void                                initSSL();
    void                                destroySSL();
private:
    ACCEPT_CALLBACK                     mAcceptCallback;
    int                                 mPort;
    bool                                mRunListen;
    std::thread*                        mListenThread;
    std::string                         mCertificate;
    std::string                         mPrivatekey;
#ifdef USE_OPENSSL
    SSL_CTX*                            mOpenSSLCTX;
#endif
};

class TcpServer
{
    typedef std::function<void (EventLoop&)>                            FRAME_CALLBACK;
    typedef std::function<void(int64_t, std::string)>                   CONNECTION_ENTER_HANDLE;
    typedef std::function<void(int64_t)>                                DISCONNECT_HANDLE;
    typedef std::function<int (int64_t, const char* buffer, int len)>   DATA_HANDLE;

public:
    TcpServer();
    ~TcpServer();

    /*  设置默认事件回调    */
    void                                setEnterHandle(TcpServer::CONNECTION_ENTER_HANDLE handle);
    void                                setDisconnectHandle(TcpServer::DISCONNECT_HANDLE handle);
    void                                setMsgHandle(TcpServer::DATA_HANDLE handle);

    TcpServer::CONNECTION_ENTER_HANDLE  getEnterHandle();
    TcpServer::DISCONNECT_HANDLE        getDisConnectHandle();
    TcpServer::DATA_HANDLE              getDataHandle();

    void                                send(int64_t id, DataSocket::PACKET_PTR&& packet);
    void                                send(int64_t id, DataSocket::PACKET_PTR& packet);

    /*主动断开此id链接，但仍然会收到此id的断开回调，需要上层逻辑自己处理这个"问题"(尽量统一在断开回调函数里做清理等工作) */
    void                                disConnect(int64_t id);

    /*  主动添加一个连接到任意IOLoop并指定事件回调, TODO::DataSocket* channel的生命周期管理   */
    void                                addDataSocket(int fd, DataSocket* channel, 
                                                        TcpServer::CONNECTION_ENTER_HANDLE enterHandle,
                                                        TcpServer::DISCONNECT_HANDLE disHandle,
                                                        TcpServer::DATA_HANDLE msgHandle);

    /*  开启监听线程  */
    void                                startListen(int port, const char *certificate = nullptr, const char *privatekey = nullptr);
    /*  开启IO工作线程    */
    void                                startWorkerThread(int threadNum, FRAME_CALLBACK callback = nullptr);

    /*  关闭服务    */
    void                                closeService();
    void                                closeListenThread();
    void                                closeWorkerThread();

    /*wakeup某id所在的网络工作线程*/
    void                                wakeup(int64_t id);
    /*wakeup 所有的网络工作线程*/
    void                                wakeupAll();

    EventLoop*                          getRandomEventLoop();
    
private:
    int64_t                             MakeID(int loopIndex);

    void                                _procDataSocketClose(DataSocket*);

private:
    EventLoop*                          mLoops;
    std::thread**                       mIOThreads;
    int                                 mLoopNum;
    bool                                mRunIOLoop;

    ListenThread                        mListenThread;

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
};

#endif