#include <time.h>
#include <stdio.h>
#include <string.h>

#include <functional>
#include <thread>
#include <iostream>
#include <exception>
#include <string>

#include "SocketLibFunction.h"

#include "EventLoop.h"
#include "DataSocket.h"

#include "TCPService.h"

static int sDefaultLoopTimeOutMS = 100;

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

TcpService::TcpService()
{
    static_assert(sizeof(SessionId) == sizeof(((SessionId*)nullptr)->id), "sizeof SessionId must equal int64_t");
    mLoops = nullptr;
    mIOThreads = nullptr;
    mIds = nullptr;
    mIncIds = nullptr;

    mEnterCallback = nullptr;
    mDisConnectCallback = nullptr;
    mDataCallback = nullptr;

    mRunIOLoop = false;
}

TcpService::~TcpService()
{
    closeService();
}

void TcpService::setEnterCallback(TcpService::ENTER_CALLBACK callback)
{
    mEnterCallback = callback;
}

void TcpService::setDisconnectCallback(TcpService::DISCONNECT_CALLBACK callback)
{
    mDisConnectCallback = callback;
}

void TcpService::setDataCallback(TcpService::DATA_CALLBACK callback)
{
    mDataCallback = callback;
}

void TcpService::send(int64_t id, DataSocket::PACKET_PTR&& packet)
{
    send(id, packet);
}

void TcpService::send(int64_t id, const DataSocket::PACKET_PTR& packet)
{
    union  SessionId sid;
    sid.id = id;
    assert(sid.data.loopIndex >= 0 && sid.data.loopIndex < mLoopNum);
    if (sid.data.loopIndex >= 0 && sid.data.loopIndex < mLoopNum)
    {
        mLoops[sid.data.loopIndex].pushAsyncProc([this, sid, packet](){
            DataSocket::PTR tmp = nullptr;
            if (mIds[sid.data.loopIndex].get(sid.data.index, tmp))
            {
                if (tmp != nullptr && tmp->getUserData() == sid.id)
                {
                    tmp->sendPacket(packet);
                }
            }
        });
    }
}

void TcpService::disConnect(int64_t id)
{
    union  SessionId sid;
    sid.id = id;
    assert(sid.data.loopIndex >= 0 && sid.data.loopIndex < mLoopNum);
    if (sid.data.loopIndex >= 0 && sid.data.loopIndex < mLoopNum)
    {
        mLoops[sid.data.loopIndex].pushAsyncProc([this, sid](){
            DataSocket::PTR tmp = nullptr;
            if (mIds[sid.data.loopIndex].get(sid.data.index, tmp))
            {
                if (tmp != nullptr && tmp->getUserData() == sid.id)
                {
                    tmp->postDisConnect();
                }
            }
        });
    }
}

void TcpService::setPingCheckTime(int64_t id, int checktime)
{
    union  SessionId sid;
    sid.id = id;
    assert(sid.data.loopIndex >= 0 && sid.data.loopIndex < mLoopNum);
    if (sid.data.loopIndex >= 0 && sid.data.loopIndex < mLoopNum)
    {
        mLoops[sid.data.loopIndex].pushAsyncProc([this, sid, checktime](){
            DataSocket::PTR tmp = nullptr;
            if (mIds[sid.data.loopIndex].get(sid.data.index, tmp))
            {
                if (tmp != nullptr && tmp->getUserData() == sid.id)
                {
                    tmp->setCheckTime(checktime);
                }
            }
        });
    }
}

void TcpService::closeService()
{
    closeListenThread();
    closeWorkerThread();
}

void TcpService::closeListenThread()
{
    mListenThread.closeListenThread();
}

void TcpService::closeWorkerThread()
{
    stopWorkerThread();

    delete[] mLoops;
    mLoops = nullptr;
    delete[] mIncIds;
    mIncIds = nullptr;
    delete[] mIds;
    mIds = nullptr;
}

void TcpService::stopWorkerThread()
{
    if (mLoops != nullptr)
    {
        mRunIOLoop = false;

        for (int i = 0; i < mLoopNum; ++i)
        {
            mLoops[i].wakeup();
        }
    }

    if (mIOThreads != nullptr)
    {
        for (int i = 0; i < mLoopNum; ++i)
        {
            mIOThreads[i]->join();
            delete mIOThreads[i];
        }

        delete[] mIOThreads;
        mIOThreads = nullptr;
    }
}

void TcpService::startListen(int port, const char *certificate, const char *privatekey)
{
    mListenThread.startListen(port, certificate, privatekey, [this](int fd){
        std::string ip = ox_socket_getipoffd(fd);
        DataSocket::PTR channel = new DataSocket(fd);
#ifdef USE_OPENSSL
        if (mListenThread.getOpenSSLCTX() != nullptr)
        {
            channel->setupAcceptSSL(mListenThread.getOpenSSLCTX());
        }
#endif
        helpAddChannel(channel, ip, mEnterCallback, mDisConnectCallback, mDataCallback);
    });
}

void TcpService::startWorkerThread(int threadNum, FRAME_CALLBACK callback)
{
    if (mLoops == nullptr)
    {
        mRunIOLoop = true;
        mLoops = new EventLoop[threadNum];
        mIOThreads = new std::thread*[threadNum];
        mLoopNum = threadNum;
        mIds = new TypeIDS<DataSocket::PTR>[threadNum];
        mIncIds = new int[threadNum];
        for (int i = 0; i < threadNum; ++i)
        {
            mIncIds[i] = 0;
        }

        for (int i = 0; i < mLoopNum; ++i)
        {
            EventLoop& l = mLoops[i];
            mIOThreads[i] = new std::thread([this, &l, callback](){
                while (mRunIOLoop)
                {
                    l.loop(sDefaultLoopTimeOutMS);
                    if (callback != nullptr)
                    {
                        callback(l);
                    }
                }
            });
        }
    }
}

void TcpService::wakeup(int64_t id)
{
    union  SessionId sid;
    sid.id = id;
    assert(sid.data.loopIndex >= 0 && sid.data.loopIndex < mLoopNum);
    if (sid.data.loopIndex >= 0 && sid.data.loopIndex < mLoopNum)
    {
        mLoops[sid.data.loopIndex].wakeup();
    }
}

void TcpService::wakeupAll()
{
    for (int i = 0; i < mLoopNum; ++i)
    {
        mLoops[i].wakeup();
    }
}

EventLoop* TcpService::getRandomEventLoop()
{
    EventLoop* ret = nullptr;
    if (mLoopNum > 0)
    {
        ret = &mLoops[rand() % mLoopNum];
    }

    return ret;
}

int64_t TcpService::MakeID(int loopIndex)
{
    union SessionId sid;
    sid.data.loopIndex = loopIndex;
    sid.data.index = mIds[loopIndex].claimID();
    sid.data.iid = mIncIds[loopIndex]++;

    return sid.id;
}

void TcpService::procDataSocketClose(DataSocket::PTR ds)
{
    int64_t id = ds->getUserData();
    union SessionId sid;
    sid.id = id;

    mIds[sid.data.loopIndex].set(nullptr, sid.data.index);
    mIds[sid.data.loopIndex].reclaimID(sid.data.index);
}

void TcpService::helpAddChannel(DataSocket::PTR channel, const std::string& ip, TcpService::ENTER_CALLBACK enterCallback, TcpService::DISCONNECT_CALLBACK disConnectCallback, TcpService::DATA_CALLBACK dataCallback)
{
    /*  随机为此链接分配一个eventloop */
    int loopIndex = rand() % mLoopNum;
    EventLoop& loop = mLoops[loopIndex];

    channel->setEnterCallback([this, loopIndex, enterCallback, disConnectCallback, dataCallback, ip](DataSocket::PTR dataSocket){
        int64_t id = MakeID(loopIndex);
        union SessionId sid;
        sid.id = id;
        mIds[loopIndex].set(dataSocket, sid.data.index);
        dataSocket->setUserData(id);
        dataSocket->setDataCallback([this, dataCallback](DataSocket::PTR ds, const char* buffer, int len){
            return dataCallback(ds->getUserData(), buffer, len);
        });

        dataSocket->setDisConnectCallback([this, disConnectCallback](DataSocket::PTR arg){
            int64_t id = arg->getUserData();
            procDataSocketClose(arg);
            disConnectCallback(id);
            delete arg;
        });

        if (enterCallback != nullptr)
        {
            enterCallback(id, ip);
        }
    });

    loop.pushAsyncProc([&, channel](){
        if (!channel->onEnterEventLoop(&loop))
        {
            delete channel;
        }
    });
}

void TcpService::addDataSocket(int fd,
                                TcpService::ENTER_CALLBACK enterCallback,
                                TcpService::DISCONNECT_CALLBACK disConnectCallback,
                                TcpService::DATA_CALLBACK dataCallback,
                                bool isUseSSL)
{
    std::string ip = ox_socket_getipoffd(fd);
    DataSocket::PTR channel = new DataSocket(fd);
#ifdef USE_OPENSSL
    if (isUseSSL)
    {
        channel->setupConnectSSL();
    }
#endif
    helpAddChannel(channel, ip, enterCallback, disConnectCallback, dataCallback);
}