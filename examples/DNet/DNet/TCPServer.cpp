#include <functional>
#include <time.h>
#include <stdio.h>
#include <thread>
#include <iostream>
#include <exception>

#include "TCPServer.h"

int
ox_socket_listen(int port, int back_num)
{
    int socketfd = SOCKET_ERROR;
    struct  sockaddr_in server_addr;
    int reuseaddr_value = 1;

    socketfd = socket(AF_INET, SOCK_STREAM, 0);

    if (socketfd != SOCKET_ERROR)
    {
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        server_addr.sin_addr.s_addr = INADDR_ANY;

        setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuseaddr_value, sizeof(int));

        if (::bind(socketfd, (struct sockaddr*)&server_addr, sizeof(struct sockaddr)) == SOCKET_ERROR ||
            listen(socketfd, back_num) == SOCKET_ERROR)
        {
            closesocket(socketfd);
            socketfd = SOCKET_ERROR;
        }
    }

    return socketfd;
}

TcpServer::TcpServer(int port, int threadNum, FRAME_CALLBACK callback)
{
    mLoops = new EventLoop[threadNum];
    mIOThreads = new std::thread*[threadNum];
    mLoopNum = threadNum;

    for (int i = 0; i < mLoopNum; ++i)
    {
        EventLoop& l = mLoops[i];
        mIOThreads[i] = new thread([&l, callback](){
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

void TcpServer::setEnterHandle(EventLoop::CONNECTION_ENTER_HANDLE handle)
{
    mEnterHandle = handle;
}

void TcpServer::setDisconnectHandle(DataSocket::DISCONNECT_PROC handle)
{
    mDisConnectHandle = handle;
}

void TcpServer::setMsgHandle(DataSocket::DATA_PROC handle)
{
    mDataProc = handle;
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
            for (;;)
            {
                while ((client_fd = accept(listen_fd, (struct sockaddr*)&socketaddress, &size)) < 0)
                {
                    if (EINTR == GetLastError())
                    {
                        continue;
                    }
                }

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
                    int rand_num = rand() % mLoopNum;
                    EventLoop& loop = mLoops[rand_num];
                    loop.addConnection(client_fd, channel, [&](Channel* arg){
                        DataSocket* ds = static_cast<DataSocket*>(arg);
                        ds->setEventLoop(&loop);

                        /*  可以放入消息队列，然后唤醒它主线程的eventloop，然后主线程通过消息队列去获取*/
                        ds->setDataHandle(mDataProc);
                        ds->setDisConnectHandle(mDisConnectHandle);
                        mEnterHandle(arg);
                    });
                }
            }

            closesocket(listen_fd);
            listen_fd = SOCKET_ERROR;
        }
        else
        {
            printf("listen failed\n");
        }
    }
}