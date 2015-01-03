#include <functional>
#include <time.h>
#include <stdio.h>
#include <thread>
#include <iostream>
#include <assert.h>

#include "eventloop.h"
#include "datasocket.h"
#include "platform.h"
#include "socketlibtypes.h"
#include "systemlib.h"

#if defined PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h> // GetTickCount()
#else
#include <unistd.h>
#include <sys/time.h> // struct timeval, gettimeofday()
#endif

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
int64_t
ox_getnowtime(void)
{
#if defined PLATFORM_WINDOWS
    int64_t second = time(NULL);
    SYSTEMTIME sys;
    GetLocalTime(&sys);
    return second * 1000 + sys.wMilliseconds;
#elif defined(ENABLE_RDTSC)
    return (unsigned int)((_rdtsc() - RDTSC_BEGINTICK) / RDTSC_CLOCK);
#elif (defined(_POSIX_TIMERS) && _POSIX_TIMERS > 0 && defined(_POSIX_MONOTONIC_CLOCK) /* posix compliant */) || (defined(__FreeBSD_cc_version) && __FreeBSD_cc_version >= 500005 /* FreeBSD >= 5.1.0 */)
    struct timespec tval;
    clock_gettime(CLOCK_MONOTONIC, &tval);
    return tval.tv_sec * 1000 + tval.tv_nsec / 1000000;
#else
    struct timeval tval;
    gettimeofday(&tval, NULL);
    return tval.tv_sec * 1000 + tval.tv_usec / 1000;
#endif
}

int
ox_socket_connect(const char* server_ip, int port)
{
    struct sockaddr_in server_addr;
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);

    if (clientfd != SOCKET_ERROR)
    {
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = inet_addr(server_ip);
        server_addr.sin_port = htons(port);

        while (connect(clientfd, (struct sockaddr*)&server_addr, sizeof(struct sockaddr)) < 0)
        {
            if (EINTR == sErrno)
            {
                continue;
            }
        }
    }

    return clientfd;
}

#include "TCPServer.h"
#include "rwlist.h"

enum NetMsgType
{
    NMT_ENTER,      /*链接进入*/
    NMT_CLOSE,      /*链接断开*/
    NMT_RECV_DATA,  /*收到消息*/
};

struct NetMsg
{
    NetMsg(NetMsgType t, int64_t id) : mType(t), mID(id)
    {
    }

    void        setData(const char* data, int len)
    {
        mData = std::string(data, len);
    }

    NetMsgType  mType;
    int64_t     mID;
    std::string      mData;
};

#include <chrono>
#include <thread>

std::mutex                      gTatisticsCppMutex;

void    lockStatistics()
{
    gTatisticsCppMutex.lock();
}

void    unLockStatistics()
{
    gTatisticsCppMutex.unlock();
}

#define TEST_PORT 5999

int main()
{
    double total_recv_len = 0;
    double  packet_num = 0;

    int client_num;
    int thread_num;
    int packet_len;

    std::cout << "enter client num:";
    std::cin >> client_num;

    std::cout << "enter thread num:";
    std::cin >> thread_num;

    std::cout << "enter packet len:";
    std::cin >> packet_len;
#ifdef PLATFORM_WINDOWS
    WSADATA g_WSAData;
    WSAStartup(MAKEWORD(2, 2), &g_WSAData);
#else
#endif
    
    int total_client_num = 0;

    Rwlist<NetMsg*>  msgList;
    EventLoop       mainLoop;

    /*  TODO::怎么优化消息队列，网络层没有不断的flush消息队列，然而又不能在每个消息发送都强制flush，当然逻辑层也可以采取主动拉的方式。去强制拉过来   */
    /*  TODO::把(每个IO线程一个消息队列)消息队列隐藏到TCPServer中。 对用户不可见    */
    /*逻辑线程对DataSocket不可见，而是采用id通信(也要确保不串话)。逻辑线程会：1，发数据。2，断开链接。  所以TCPServer的loop要每个循环处理消息队列*/
    TcpServer t(TEST_PORT, thread_num, [&](EventLoop& l){
        /*每帧回调函数里强制同步rwlist*/
        if (true)
        {
            lockStatistics();
            msgList.ForceSyncWrite();
            unLockStatistics();

            if (msgList.SharedListSize() > 0)
            {
                mainLoop.wakeup();
            }
        }
    });
    t.setEnterHandle([&](int64_t id){
        if (true)
        {
            NetMsg* msg = new NetMsg(NMT_ENTER, id);
            lockStatistics();
            msgList.Push(msg);
            unLockStatistics();

            mainLoop.wakeup();
        }
        else
        {
            lockStatistics();
            total_client_num++;
            unLockStatistics();
        }
    });

    t.setDisconnectHandle([&](int64_t id){
        if (true)
        {
            NetMsg* msg = new NetMsg(NMT_CLOSE, id);
            lockStatistics();
            msgList.Push(msg);
            unLockStatistics();

            mainLoop.wakeup();
        }
        else
        {
            lockStatistics();
            total_client_num--;
            unLockStatistics();
        }
    });

    t.setMsgHandle([&](int64_t id, const char* buffer, int len){
        if (true)
        {
            NetMsg* msg = new NetMsg(NMT_RECV_DATA, id);
            msg->setData(buffer, len);
            lockStatistics();
            msgList.Push(msg);
            unLockStatistics();

            mainLoop.wakeup();
        }
        else
        {
            t.send(id, DataSocket::makePacket(buffer, len));
            lockStatistics();
            total_recv_len += len;
            packet_num++;
            unLockStatistics();
            
        }
    });

    /*  客户端IO线程   */
    for (int i = 0; i < thread_num; ++i)
    {
        new std::thread([&mainLoop, &client_num, &packet_len]{
            printf("start one client thread \n");
            /*  客户端eventloop*/
            EventLoop clientEventLoop;

            /*  消息包大小定义 */
            const char* senddata = (const char*)malloc(packet_len);

            for (int i = 0; i < client_num; i++)
            {
                int client = ox_socket_connect("127.0.0.1", TEST_PORT);
                if (client == SOCKET_ERROR)
                {
                    printf("error : %d \n", sErrno);
                }
                printf("connect fd: %d\n", client);
                int flag = 1;
                int setret = setsockopt(client, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(flag));
                std::cout << setret << std::endl;

                clientEventLoop.addConnection(client, new DataSocket(client), [&](Channel* arg){
                    DataSocket* ds = static_cast<DataSocket*>(arg);
                    /*  发送消息    */
                    ds->send(senddata, packet_len);

                    /*  可以放入消息队列，然后唤醒它主线程的eventloop，然后主线程通过消息队列去获取*/
                    ds->setDataHandle([](DataSocket* ds, const char* buffer, int len){
                        ds->send(buffer, len);
                    });
                });
            }

            while (true)
            {
                clientEventLoop.loop(-1);
            }
        });
    }

    /*  主线程处理服务器端IO*/
    int64_t lasttime = ox_getnowtime();

    while (true)
    {
        mainLoop.loop(10);

        msgList.SyncRead(0);
        NetMsg* msg = nullptr;
        while (msgList.ReadListSize() > 0)
        {
            bool ret = msgList.PopFront(&msg);
            if (ret)
            {
                if (msg->mType == NMT_ENTER)
                {
                    printf("client %lld enter \n", msg->mID);
                    total_client_num++;
                }
                else if (msg->mType == NMT_CLOSE)
                {
                    printf("client %lld close \n", msg->mID);
                    total_client_num--;
                }
                else if (msg->mType == NMT_RECV_DATA)
                {
                    t.send(msg->mID, DataSocket::makePacket(msg->mData.c_str(), msg->mData.size()));
                    total_recv_len += msg->mData.size();
                    packet_num++;
                }
                else
                {
                    assert(false);
                }

                delete msg;
                msg = nullptr;
            }
            else
            {
                break;
            }
        }
        int64_t now = ox_getnowtime();
        if ((now - lasttime) >= 1000)
        {
            std::cout << "recv by clientnum:" << total_client_num << " of :" << (total_recv_len / 1024) / 1024 << " M / s, " << "packet num : " << packet_num << std::endl;
            lasttime = now;
            total_recv_len = 0;
            packet_num = 0;
        }
    }
}