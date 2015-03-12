#include <functional>
#include <time.h>
#include <stdio.h>
#include <thread>
#include <iostream>
#include <assert.h>
#include <chrono>

#include "socketlibfunction.h"

#include "eventloop.h"
#include "datasocket.h"
#include "httprequest.h"
#include "timer.h"

void SSL_init()
{
    /* SSL 库初始化 */
    SSL_library_init();
    /* 载入所有 SSL 算法 */
    OpenSSL_add_all_algorithms();
    /* 载入所有 SSL 错误消息 */
    SSL_load_error_strings();
}

static void fuck_send(TimerMgr* tm, DataSocket* ds, const char* buffer, int len)
{
    ds->send(buffer, len);
    tm->AddTimer(200, fuck_send, tm, ds, buffer, len);
}

int main()
{
    SSL_init();

    double total_recv_len = 0;
    double  packet_num = 0;

    int client_num;
    int thread_num;
    int packet_len;

    std::cout << "enter client(one thread) num:";
    std::cin >> client_num;

    std::cout << "enter thread num:";
    std::cin >> thread_num;

    std::cout << "enter packet len:";
    std::cin >> packet_len;

    int port_num;
    std::cout << "enter port:";
    std::cin >> port_num;

    ox_socket_init();

    EventLoop       mainLoop;

    /*  客户端IO线程   */

    std::thread** ts = new std::thread*[thread_num];

    for (int i = 0; i < thread_num; ++i)
    {
        ts[i] = new std::thread([&mainLoop, client_num, packet_len, port_num]{
            printf("start one client thread \n");
            /*  客户端eventloop*/
            EventLoop clientEventLoop;

            /*  消息包大小定义 */
            char* senddata = nullptr;
            if (packet_len > 0)
            {
                senddata = (char*)malloc(packet_len);
                senddata[packet_len - 1] = 0;
            }

            int64_t total_recv = 0;
            TimerMgr tm;
#if 0
            for (int i = 0; i < client_num; i++)
            {
                /*int client = ox_socket_connect("180.97.33.107", port_num);*/
                int client = ox_socket_nonblockconnect("180.97.33.107", port_num);
                if (client == SOCKET_ERROR)
                {
                    printf("error : %d \n", sErrno);
                }
                printf("connect fd: %d\n", client);
                int flag = 1;
                int setret = setsockopt(client, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(flag));
                std::cout << setret << std::endl;

                DataSocket* pClient = new DataSocket(client);
                pClient->setupConnectSSL();

                clientEventLoop.addChannel(client, pClient, [&](Channel* arg){
                    DataSocket* ds = static_cast<DataSocket*>(arg);
                    /*  发送消息    */
                    HttpRequest hr;
                    hr.setHost("www.baidu.com");
                    hr.setProtocol(HRP_GET);
                    hr.setRequestUrl("/");
                    ds->send(hr.getResult().c_str(), hr.getResult().size());

                    /*  可以放入消息队列，然后唤醒它主线程的eventloop，然后主线程通过消息队列去获取*/
                    ds->setDataHandle([](DataSocket* ds, const char* buffer, int len){
                        ds->send(buffer, len);
                        return len;
                    });

                    ds->setDisConnectHandle([](DataSocket* arg){
                        delete arg;
                    });
                });
            }
#else
            for (int i = 0; i < client_num; i++)
            {
                /*int client = ox_socket_connect("180.97.33.107", port_num);*/
                int client = ox_socket_connect("127.0.0.1", port_num);
                if (client == SOCKET_ERROR)
                {
                    printf("error : %d \n", sErrno);
                }
                printf("connect fd: %d\n", client);
                int flag = 1;
                int setret = setsockopt(client, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(flag));
                std::cout << setret << std::endl;

                DataSocket* pClient = new DataSocket(client);

                clientEventLoop.addChannel(client, pClient, [&](Channel* arg){
                    DataSocket* ds = static_cast<DataSocket*>(arg);
                    if (senddata != nullptr)
                    {
                        fuck_send(&tm, ds, senddata, packet_len);
                    }

                    /*  可以放入消息队列，然后唤醒它主线程的eventloop，然后主线程通过消息队列去获取*/
                    ds->setDataHandle([&total_recv](DataSocket* ds, const char* buffer, int len){
                        //ds->send(buffer, len);
                        total_recv += len;
                        return len;
                    });

                    ds->setDisConnectHandle([](DataSocket* arg){
                        delete arg;
                    });
                });
            }
#endif

            int64_t now = ox_getnowtime();
            while (true)
            {
                clientEventLoop.loop(tm.NearEndMs());
                tm.Schedule();
                if ((ox_getnowtime() - now) >= 1000)
                {
                    cout << "total recv:" << (total_recv / 1024) / 1024 << " M /s" << endl;
                    now = ox_getnowtime();
                    total_recv = 0;
                }
            }
        });
    }

    for (int i = 0; i < thread_num; ++i)
    {
        ts[i]->join();
    }
}