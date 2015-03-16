#include <time.h>
#include <stdio.h>
#include <string.h>

#include <functional>
#include <thread>
#include <iostream>
#include <exception>

#include "socketlibfunction.h"
#include "socketlibtypes.h"

#include "eventloop.h"
#include "datasocket.h"

#include "TCPServer.h"

TcpServer::TcpServer()
{
    static_assert(sizeof(SessionId) == sizeof(((SessionId*)nullptr)->id), "sizeof SessionId must equal int64_t");
    mRunService = false;
    mLoops = nullptr;
    mIOThreads = nullptr;
    mListenThread = nullptr;
    mIds = nullptr;
    mIncIds = nullptr;
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
    if (mRunService)
    {
        mRunService = false;

        sock tmp = ox_socket_connect("127.0.0.1", mPort);
        ox_socket_close(tmp);
        tmp = SOCKET_ERROR;

        mListenThread->join();
        delete mListenThread;
        mListenThread = NULL;

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

void TcpServer::startService(int port, const char *certificate, const char *privatekey, int threadNum, FRAME_CALLBACK callback /* = nullptr */)
{
    if (!mRunService)
    {
        mRunService = true;

        mPort = port;
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
                while (mRunService)
                {
                    l.loop(-1);
                    if (callback != nullptr)
                    {
                        callback(l);
                    }
                }
            });
        }

        if (certificate != nullptr)
        {
            mCertificate = certificate;
        }
        if (privatekey != nullptr)
        {
            mPrivatekey = privatekey;
        }

        mListenThread = new std::thread([this, port](){
            RunListen(port);
        });
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

void TcpServer::RunListen(int port)
{
    sock client_fd = SOCKET_ERROR;
    struct sockaddr_in socketaddress;
    socklen_t size = sizeof(struct sockaddr);

    sock listen_fd = ox_socket_listen(port, 25);
#ifdef USE_OPENSSL
    SSL_CTX *ctx = nullptr;

    if (!mCertificate.empty() && !mPrivatekey.empty())
    {
        ctx = SSL_CTX_new(SSLv23_server_method());
        if (SSL_CTX_use_certificate_file(ctx, mCertificate.c_str(), SSL_FILETYPE_PEM) <= 0) {
            SSL_CTX_free(ctx);
            ctx = nullptr;
        }
        /* 载入用户私钥 */
        if (SSL_CTX_use_PrivateKey_file(ctx, mPrivatekey.c_str(), SSL_FILETYPE_PEM) <= 0) {
            SSL_CTX_free(ctx);
            ctx = nullptr;
        }
        /* 检查用户私钥是否正确 */
        if (!SSL_CTX_check_private_key(ctx)) {
            SSL_CTX_free(ctx);
            ctx = nullptr;
        }
    }
#endif

    if (SOCKET_ERROR != listen_fd)
    {
        printf("listen : %d \n", port);
        for (;mRunService;)
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
                int flag = 1;
                setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(flag));
                {
                    int sd_size = 32 * 1024;
                    int op_size = sizeof(sd_size);
                    setsockopt(client_fd, SOL_SOCKET, SO_SNDBUF, (const char*)&sd_size, op_size);
                }

                DataSocket* channel = new DataSocket(client_fd);
#ifdef USE_OPENSSL
                if (ctx != nullptr)
                {
                    channel->setupAcceptSSL(ctx);
                }
#endif
                int loopIndex = rand() % mLoopNum;
                EventLoop& loop = mLoops[loopIndex];
                /*  随机为此链接分配一个eventloop */
                loop.addChannel(client_fd, channel, [this, loopIndex](Channel* arg){
                    /*  当eventloop接管此链接后，执行此lambda回调    */
                    DataSocket* ds = static_cast<DataSocket*>(arg);

                    int64_t id = MakeID(loopIndex);
                    union SessionId sid;
                    sid.id = id;
                    mIds[loopIndex].set(ds, sid.data.index);
                    ds->setUserData(id);
                    ds->setDataHandle([this](DataSocket* ds, const char* buffer, int len){
                        return mDataHandle(ds->getUserData(), buffer, len);
                    });

                    ds->setDisConnectHandle([this](DataSocket* arg){
                        /*TODO::这里也需要一开始就保存this指针， delete datasocket之后，此lambda析构，绑定变量失效，所以最后继续访问this就会宕机*/
                        TcpServer* p = this;
                        int64_t id = arg->getUserData();
                        p->_procDataSocketClose(arg);
                        p->mDisConnectHandle(id);
                    });

                    mEnterHandle(id);
                });
            }
        }

#ifdef PLATFORM_WINDOWS
        closesocket(listen_fd);
#else
        close(listen_fd);
#endif
        listen_fd = SOCKET_ERROR;
    }
    else
    {
        printf("listen failed, error:%d \n", sErrno);
        return;
    }
}