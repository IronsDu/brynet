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

TcpServer::TcpServer(int port, int threadNum, FRAME_CALLBACK callback)
{
    static_assert(sizeof(SessionId) == sizeof(((SessionId*)nullptr)->id), "sizeof SessionId must equal int64_t");

    mLoops = new EventLoop[threadNum];
    mIOThreads = new std::thread*[threadNum];
    mLoopNum = threadNum;
    mIds = new TypeIDS<DataSocket>[threadNum];
    mIncIds = new int[threadNum];
    memset(mIncIds, 0, sizeof(mIncIds[0])*threadNum);

    for (int i = 0; i < mLoopNum; ++i)
    {
        EventLoop& l = mLoops[i];
        mIOThreads[i] = new std::thread([&l, callback](){
            l.restoreThreadID();
            while (true)
            {
                l.loop(-1);
                if (callback != nullptr)
                {
                    callback(l);
                }
            }
        });
    }
    
    mListenThread = new std::thread([this, port](){
        RunListen(port);
    });
}

TcpServer::~TcpServer()
{
    for (int i = 0; i < mLoopNum; ++i)
    {
        mIOThreads[i]->join();
        delete mIOThreads[i];
    }

    delete[] mIOThreads;

    mListenThread->join();
    delete mListenThread;
    mListenThread = NULL;

    delete[] mLoops;
    mLoops = NULL;
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

    if (mIOThreads[sid.data.loopIndex]->get_id() != std::this_thread::get_id())
    {
        /*TODO::pushAsyncProc有效率问题。未知 */
        mLoops[sid.data.loopIndex].pushAsyncProc([this, sid, packet](){

            DataSocket* ds = mIds[sid.data.loopIndex].get(sid.data.index);
            if (ds != nullptr && ds->getUserData() == sid.id)
            {
                ds->sendPacket(packet);
            }

        });
    }
    else
    {
        DataSocket* ds = mIds[sid.data.loopIndex].get(sid.data.index);
        if (ds != nullptr && ds->getUserData() == sid.id)
        {
            ds->sendPacket(packet);
        }
    }
}

void TcpServer::disConnect(int64_t id)
{
    union  SessionId sid;
    sid.id = id;

    if (mIOThreads[sid.data.loopIndex]->get_id() != std::this_thread::get_id())
    {
        /*TODO::pushAsyncProc有效率问题。未知 */
        mLoops[sid.data.loopIndex].pushAsyncProc([this, sid](){

            DataSocket* ds = mIds[sid.data.loopIndex].get(sid.data.index);
            if (ds != nullptr && ds->getUserData() == sid.id)
            {
                ds->postDisConnect();
            }

        });
    }
    else
    {
        DataSocket* ds = mIds[sid.data.loopIndex].get(sid.data.index);
        if (ds != nullptr && ds->getUserData() == sid.id)
        {
            ds->postDisConnect();
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

void TcpServer::RunListen(int port)
{
    while (true)
    {
        int client_fd = SOCKET_ERROR;
        struct sockaddr_in socketaddress;
        socklen_t size = sizeof(struct sockaddr);

        int listen_fd = ox_socket_listen(port, 25);

        if (SOCKET_ERROR != listen_fd)
        {
            printf("listen : %d \n", port);
            for (;;)
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

                    Channel* channel = new DataSocket(client_fd);
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
                            mDataHandle(ds->getUserData(), buffer, len);
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
}