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

void SSL_init()
{
    /* SSL 库初始化 */
    SSL_library_init();
    /* 载入所有 SSL 算法 */
    OpenSSL_add_all_algorithms();
    /* 载入所有 SSL 错误消息 */
    SSL_load_error_strings();
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

            for (int i = 0; i < client_num; i++)
            {
                int client = ox_socket_connect("180.97.33.107", port_num);
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

            while (true)
            {
                clientEventLoop.loop(-1);
            }
        });
    }

    for (int i = 0; i < thread_num; ++i)
    {
        ts[i]->join();
    }
}