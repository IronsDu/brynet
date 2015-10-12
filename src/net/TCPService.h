#ifndef _TCP_SERVER_H
#define _TCP_SERVER_H

#include <vector>
#include <string>
#include <functional>
#include <thread>
#include <assert.h>
#include <stdint.h>
#include <memory>

#include "DataSocket.h"
#include "typeids.h"

class EventLoop;
class DataSocket;

class ListenThread
{
public:
    typedef std::shared_ptr<ListenThread>   PTR;

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

class TcpService
{
public:
    typedef std::shared_ptr<TcpService>                                 PTR;

    typedef std::function<void (EventLoop&)>                            FRAME_CALLBACK;
    typedef std::function<void(int64_t, std::string)>                   ENTER_CALLBACK;
    typedef std::function<void(int64_t)>                                DISCONNECT_CALLBACK;
    typedef std::function<int (int64_t, const char* buffer, int len)>   DATA_CALLBACK;

public:
    TcpService();
    ~TcpService();

    /*  设置默认事件回调    */
    void                                setEnterCallback(TcpService::ENTER_CALLBACK callback);
    void                                setDisconnectCallback(TcpService::DISCONNECT_CALLBACK callback);
    void                                setDataCallback(TcpService::DATA_CALLBACK callback);

    void                                send(int64_t id, DataSocket::PACKET_PTR&& packet);
    void                                send(int64_t id, const DataSocket::PACKET_PTR& packet);

    /*  逻辑线程调用，将要发送的消息包缓存起来，再一次性通过flushCachePackectList放入到网络线程    */
    void                                cacheSend(int64_t id, DataSocket::PACKET_PTR&& packet);
    void                                cacheSend(int64_t id, const DataSocket::PACKET_PTR& packet);

    void                                flushCachePackectList();

    /*主动断开此id链接，但仍然会收到此id的断开回调，需要上层逻辑自己处理这个"问题"(尽量统一在断开回调函数里做清理等工作) */
    void                                disConnect(int64_t id);

    void                                setPingCheckTime(int64_t id, int checktime);

    void                                addDataSocket(  int fd,
                                                        TcpService::ENTER_CALLBACK enterCallback,
                                                        TcpService::DISCONNECT_CALLBACK disConnectCallback,
                                                        TcpService::DATA_CALLBACK dataCallback,
                                                        bool isUseSSL);

    /*  开启监听线程  */
    void                                startListen(int port, const char *certificate = nullptr, const char *privatekey = nullptr);
    /*  开启IO工作线程    */
    void                                startWorkerThread(int threadNum, FRAME_CALLBACK callback = nullptr);

    /*  关闭服务(且清理内存):非线程安全    */
    void                                closeService();
    void                                closeListenThread();
    void                                closeWorkerThread();

    /*  仅仅是停止工作线程以及让每个EventLoop退出循环，但不释放EventLoop内存 */
    void                                stopWorkerThread();

    /*  wakeup某id所在的网络工作线程:非线程安全    */
    void                                wakeup(int64_t id);
    /*  wakeup 所有的网络工作线程:非线程安全  */
    void                                wakeupAll();
    /*  随机获取一个EventLoop:非线程安全   */
    EventLoop*                          getRandomEventLoop();
    
private:
    void                                helpAddChannel(DataSocket::PTR channel, 
                                                    const std::string& ip,
                                                    TcpService::ENTER_CALLBACK enterCallback,
                                                    TcpService::DISCONNECT_CALLBACK disConnectCallback,
                                                    TcpService::DATA_CALLBACK dataCallback);
private:
    int64_t                             MakeID(int loopIndex);

    void                                procDataSocketClose(DataSocket::PTR);

private:
    typedef std::vector<std::pair<int64_t, DataSocket::PACKET_PTR>> MSG_LIST;
    std::shared_ptr<MSG_LIST>*          mCachePacketList;
    EventLoop*                          mLoops;
    std::thread**                       mIOThreads;
    int                                 mLoopNum;
    bool                                mRunIOLoop;

    ListenThread                        mListenThread;

    TypeIDS<DataSocket::PTR>*           mIds;
    int*                                mIncIds;

    /*  以下三个回调函数会在多线程中调用(每个线程即一个eventloop驱动的io loop)(见：RunListen中的使用)   */
    TcpService::ENTER_CALLBACK           mEnterCallback;
    TcpService::DISCONNECT_CALLBACK      mDisConnectCallback;
    TcpService::DATA_CALLBACK            mDataCallback;

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
