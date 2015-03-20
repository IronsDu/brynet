#include <time.h>
#include <stdio.h>
#include <string.h>

#include <functional>
#include <thread>
#include <iostream>
#include <exception>
#include <string>

#include "socketlibfunction.h"
#include "socketlibtypes.h"

#include "eventloop.h"
#include "datasocket.h"

#include "TCPServer.h"

ListenThread::ListenThread()
{
    mAcceptCallback = nullptr;
    mPort = 0;
    mRunListen = false;
    mListenThread = nullptr;
#ifdef USE_OPENSSL
    mOpenSSLCTX = nullptr;
#endif
}

ListenThread::~ListenThread()
{
    closeListenThread();
}

void ListenThread::startListen(int port, const char *certificate, const char *privatekey, ACCEPT_CALLBACK callback)
{
    if (!mRunListen)
    {
        mRunListen = true;
        mPort = port;
        mAcceptCallback = callback;
        if (certificate != nullptr)
        {
            mCertificate = certificate;
        }
        if (privatekey != nullptr)
        {
            mPrivatekey = privatekey;
        }

        mListenThread = new std::thread([this](){
            RunListen();
        });
    }
}

void ListenThread::closeListenThread()
{
    if (mListenThread != nullptr)
    {
        mRunListen = false;

        sock tmp = ox_socket_connect("127.0.0.1", mPort);
        ox_socket_close(tmp);
        tmp = SOCKET_ERROR;

        mListenThread->join();
        delete mListenThread;
        mListenThread = NULL;
    }
}

#ifdef USE_OPENSSL
SSL_CTX* ListenThread::getOpenSSLCTX()
{
    return mOpenSSLCTX;
}
#endif

void ListenThread::initSSL()
{
#ifdef USE_OPENSSL
    mOpenSSLCTX = nullptr;

    if (!mCertificate.empty() && !mPrivatekey.empty())
    {
        mOpenSSLCTX = SSL_CTX_new(SSLv23_server_method());
        if (SSL_CTX_use_certificate_file(mOpenSSLCTX, mCertificate.c_str(), SSL_FILETYPE_PEM) <= 0) {
            SSL_CTX_free(mOpenSSLCTX);
            mOpenSSLCTX = nullptr;
        }
        /* 载入用户私钥 */
        if (SSL_CTX_use_PrivateKey_file(mOpenSSLCTX, mPrivatekey.c_str(), SSL_FILETYPE_PEM) <= 0) {
            SSL_CTX_free(mOpenSSLCTX);
            mOpenSSLCTX = nullptr;
        }
        /* 检查用户私钥是否正确 */
        if (!SSL_CTX_check_private_key(mOpenSSLCTX)) {
            SSL_CTX_free(mOpenSSLCTX);
            mOpenSSLCTX = nullptr;
        }
    }
#endif
}

void ListenThread::destroySSL()
{
#ifdef USE_OPENSSL
    if(mOpenSSLCTX != nullptr)
    {
        SSL_CTX_free(mOpenSSLCTX);
        mOpenSSLCTX = nullptr;
    }
#endif
}

void ListenThread::RunListen()
{
    sock client_fd = SOCKET_ERROR;
    struct sockaddr_in socketaddress;
    socklen_t size = sizeof(struct sockaddr);

    sock listen_fd = ox_socket_listen(mPort, 25);
    initSSL();

    if (SOCKET_ERROR != listen_fd)
    {
        printf("listen : %d \n", mPort);
        for (; mRunListen;)
        {
            while ((client_fd = accept(listen_fd, (struct sockaddr*)&socketaddress, &size)) < 0)
            {
                if (EINTR == sErrno)
                {
                    continue;
                }
            }

            printf("accept fd : %d \n", client_fd);

            if (SOCKET_ERROR != client_fd)
            {
                ox_socket_nodelay(client_fd);
                ox_socket_setsdsize(client_fd, 32 * 1024);
                mAcceptCallback(client_fd);
            }
        }

        ox_socket_close(listen_fd);
        listen_fd = SOCKET_ERROR;
    }
    else
    {
        printf("listen failed, error:%d \n", sErrno);
        return;
    }
}

TcpServer::TcpServer()
{
    static_assert(sizeof(SessionId) == sizeof(((SessionId*)nullptr)->id), "sizeof SessionId must equal int64_t");
    mLoops = nullptr;
    mIOThreads = nullptr;
    mIds = nullptr;
    mIncIds = nullptr;

    mEnterHandle = nullptr;
    mDisConnectHandle = nullptr;
    mDataHandle = nullptr;

    mRunIOLoop = false;
}

TcpServer::~TcpServer()
{
    closeService();
}

void TcpServer::setEnterHandle(TcpServer::CONNECTION_ENTER_HANDLE handle)
{
    mEnterHandle = handle;
}

void TcpServer::setDisconnectHandle(TcpServer::DISCONNECT_HANDLE handle)
{
    mDisConnectHandle = handle;
}

void TcpServer::setMsgHandle(TcpServer::DATA_HANDLE handle)
{
    mDataHandle = handle;
}

TcpServer::CONNECTION_ENTER_HANDLE TcpServer::getEnterHandle()
{
    return mEnterHandle;
}

TcpServer::DISCONNECT_HANDLE TcpServer::getDisConnectHandle()
{
    return mDisConnectHandle;
}

TcpServer::DATA_HANDLE TcpServer::getDataHandle()
{
    return mDataHandle;
}

void TcpServer::send(int64_t id, DataSocket::PACKET_PTR&& packet)
{
    send(id, packet);
}

void TcpServer::send(int64_t id, DataSocket::PACKET_PTR& packet)
{
    union  SessionId sid;
    sid.id = id;

    mLoops[sid.data.loopIndex].pushAsyncProc([this, sid, packet](){

        DataSocket* ds = mIds[sid.data.loopIndex].get(sid.data.index);
        if (ds != nullptr && ds->getUserData() == sid.id)
        {
            ds->sendPacket(packet);
        }

    });
}

void TcpServer::disConnect(int64_t id)
{
    union  SessionId sid;
    sid.id = id;

    mLoops[sid.data.loopIndex].pushAsyncProc([this, sid](){

        DataSocket* ds = mIds[sid.data.loopIndex].get(sid.data.index);
        if (ds != nullptr && ds->getUserData() == sid.id)
        {
            ds->postDisConnect();
        }
    });
}

void TcpServer::closeService()
{
    closeListenThread();
    closeWorkerThread();
}

void TcpServer::closeListenThread()
{
    mListenThread.closeListenThread();
}

void TcpServer::closeWorkerThread()
{
    if (mLoops != nullptr)
    {
        mRunIOLoop = false;

        for (int i = 0; i < mLoopNum; ++i)
        {
            mLoops[i].wakeup();
        }

        for (int i = 0; i < mLoopNum; ++i)
        {
            mIOThreads[i]->join();
            delete mIOThreads[i];
        }

        delete[] mIOThreads;
        mIOThreads = nullptr;
        delete[] mLoops;
        mLoops = nullptr;
        delete[] mIncIds;
        mIncIds = nullptr;
        delete[] mIds;
        mIds = nullptr;
    }
}

void TcpServer::startListen(int port, const char *certificate, const char *privatekey)
{
    mListenThread.startListen(port, certificate, privatekey, [this](int fd){
        DataSocket* channel = new DataSocket(fd);
#ifdef USE_OPENSSL
        if (mListenThread.getOpenSSLCTX() != nullptr)
        {
            channel->setupAcceptSSL(mListenThread.getOpenSSLCTX());
        }
#endif
        addDataSocket(fd, channel, mEnterHandle, mDisConnectHandle, mDataHandle);
    });
}

void TcpServer::startWorkerThread(int threadNum, FRAME_CALLBACK callback)
{
    if (mLoops == nullptr)
    {
        mRunIOLoop = true;
        mLoops = new EventLoop[threadNum];
        mIOThreads = new std::thread*[threadNum];
        mLoopNum = threadNum;
        mIds = new TypeIDS<DataSocket>[threadNum];
        mIncIds = new int[threadNum];
        memset(mIncIds, 0, sizeof(mIncIds[0])*threadNum);

        for (int i = 0; i < mLoopNum; ++i)
        {
            EventLoop& l = mLoops[i];
            mIOThreads[i] = new std::thread([this, &l, callback](){
                l.restoreThreadID();
                while (mRunIOLoop)
                {
                    l.loop(100);
                    if (callback != nullptr)
                    {
                        callback(l);
                    }
                }
            });
        }
    }
}

int64_t TcpServer::MakeID(int loopIndex)
{
    union SessionId sid;
    sid.data.loopIndex = loopIndex;
    sid.data.index = mIds[loopIndex].claimID();
    sid.data.iid = mIncIds[loopIndex]++;

    return sid.id;
}

void TcpServer::_procDataSocketClose(DataSocket* ds)
{
    int64_t id = ds->getUserData();
    union SessionId sid;
    sid.id = id;

    mIds[sid.data.loopIndex].set(nullptr, sid.data.index);
    mIds[sid.data.loopIndex].reclaimID(sid.data.index);
    delete ds;
}

void TcpServer::addDataSocket(int fd, DataSocket* channel, 
                                TcpServer::CONNECTION_ENTER_HANDLE enterHandle,
                                TcpServer::DISCONNECT_HANDLE disHandle,
                                TcpServer::DATA_HANDLE msgHandle)
{
    int loopIndex = rand() % mLoopNum;
    EventLoop& loop = mLoops[loopIndex];
    /*  随机为此链接分配一个eventloop */
    std::string ip = ox_socket_getipoffd(fd);
    loop.addChannel(fd, channel, [this, loopIndex, enterHandle, disHandle, msgHandle, ip](Channel* arg){
        /*  当eventloop接管此链接后，执行此lambda回调    */
        DataSocket* ds = static_cast<DataSocket*>(arg);

        int64_t id = MakeID(loopIndex);
        union SessionId sid;
        sid.id = id;
        mIds[loopIndex].set(ds, sid.data.index);
        ds->setUserData(id);
        ds->setDataHandle([this, msgHandle](DataSocket* ds, const char* buffer, int len){
            return msgHandle(ds->getUserData(), buffer, len);
        });

        ds->setDisConnectHandle([this, disHandle](DataSocket* arg){
            /*TODO::这里也需要一开始就保存this指针， delete datasocket之后，此lambda析构，绑定变量失效，所以最后继续访问this就会宕机*/
            TcpServer::DISCONNECT_HANDLE tmp = disHandle;
            TcpServer* p = this;
            int64_t id = arg->getUserData();
            p->_procDataSocketClose(arg);
            tmp(id);
        });
        
        if (enterHandle != nullptr)
        {
            enterHandle(id, ip);
        }
    });
}