#include <functional>
#include <time.h>
#include <stdio.h>
#include <thread>
#include <iostream>

#include "eventloop.h"
#include "datasocket.h"

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
            if (EINTR == GetLastError())
            {
                continue;
            }
        }
    }

    return clientfd;
}

#include "TCPServer.h"
#include "rwlist.h"

struct NetMsg
{
    NetMsg(int t, Channel* c) : mType(t), mChannel(c)
    {
    }

    void        setData(const char* data, int len)
    {
        mData = string(data, len);
    }

    int         mType;
    Channel*    mChannel;
    string      mData;
};

int main()
{
    double total_recv_len = 0;
    double  packet_num = 0;

    WSADATA g_WSAData;
    WSAStartup(MAKEWORD(2, 2), &g_WSAData);

    Rwlist<NetMsg*>  msgList;
    EventLoop       mainLoop;

    /*  TODO::怎么优化消息队列，网络层没有不断的flush消息队列，然而又不能在每个消息发送都强制flush，当然逻辑层也可以采取主动拉的方式。去强制拉过来   */
    /*  TODO::把(每个IO线程一个消息队列)消息队列隐藏到TCPServer中。 对用户不可见    */
    /*逻辑线程对DataSocket不可见，而是采用id通信(也要确保不串话)。逻辑线程会：1，发数据。2，断开链接。  所以TCPServer的loop要每个循环处理消息队列*/
    TcpServer t(8888, 1);
    t.setEnterHandle([&](Channel* c){
        printf("enter client \n");
        /*
                NetMsg* msg = new NetMsg(1, c);
                msgList.Push(msg);
                mainLoop.wakeup();
                */
    });

    t.setDisconnectHandle([](DataSocket* c){
        printf("client dis connect \n");
        delete c;

    });

    t.setMsgHandle([&](DataSocket* d, const char* buffer, int len){
        //printf("recv data \n");
        /*
        NetMsg* msg = new NetMsg(2, d);
        msg->setData(buffer, len);
        msgList.Push(msg);
        mainLoop.wakeup();
        */
        d->send(buffer, len);
        total_recv_len += len;
        packet_num++;
    });

    /*  客户端IO线程   */
    for (int i = 0; i < 1; ++i)
    {
        new std::thread([&mainLoop]{
            /*  客户端eventloop*/
            EventLoop clientEventLoop;

            /*  消息包大小定义 */
#define PACKET_LEN (1)
#define CLIENT_NUM (1000)

            const char* senddata = (const char*)malloc(PACKET_LEN);

            for (int i = 0; i < CLIENT_NUM; i++)
            {
                int client = ox_socket_connect("127.0.0.1", 8888);
                int flag = 1;
                int setret = setsockopt(client, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(flag));
                cout << setret << endl;

                clientEventLoop.addConnection(client, new DataSocket(client), [&](Channel* arg){
                    DataSocket* ds = static_cast<DataSocket*>(arg);
                    ds->setEventLoop(&clientEventLoop);

                    /*  发送消息    */
                    ds->send(senddata, PACKET_LEN);

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
    DWORD lasttime = GetTickCount();
    int total_fps = 0;

    while (true)
    {
        mainLoop.loop(10);
        total_fps++;
        msgList.ForceSyncWrite();
        msgList.SyncRead(0);

        NetMsg* msg = NULL;
        while (msgList.ReadListSize() > 0)
        {
            msg = msgList.PopBack();
            delete msg;
        }

        DWORD now = GetTickCount();
        if ((now - lasttime) >= 1000)
        {
            cout << "recv :" << (total_recv_len / 1024) / 1024 << " M/s, " << "packet num: " << packet_num  << endl;
            cout << "fps:" << total_fps << endl;
            total_fps = 0;
            lasttime = now;
            total_recv_len = 0;
            packet_num = 0;
        }
    }
}