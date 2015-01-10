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

int main()
{
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

    /*  TODO::怎么优化消息队列，网络层没有不断的flush消息队列，然而又不能在每个消息发送都强制flush，当然逻辑层也可以采取主动拉的方式。去强制拉过来   */
    /*  TODO::把(每个IO线程一个消息队列)消息队列隐藏到TCPServer中。 对用户不可见    */
    /*逻辑线程对DataSocket不可见，而是采用id通信(也要确保不串话)。逻辑线程会：1，发数据。2，断开链接。  所以TCPServer的loop要每个循环处理消息队列*/
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
                int client = ox_socket_connect("127.0.0.1", port_num);
                if (client == SOCKET_ERROR)
                {
                    printf("error : %d \n", sErrno);
                }
                printf("connect fd: %d\n", client);
                int flag = 1;
                int setret = setsockopt(client, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(flag));
                std::cout << setret << std::endl;

                clientEventLoop.addChannel(client, new DataSocket(client), [&](Channel* arg){
                    DataSocket* ds = static_cast<DataSocket*>(arg);
                    /*  发送消息    */
                    if (senddata != nullptr)
                    {
                        ds->send(senddata, packet_len);
                    }

                    /*  可以放入消息队列，然后唤醒它主线程的eventloop，然后主线程通过消息队列去获取*/
                    ds->setDataHandle([](DataSocket* ds, const char* buffer, int len){
                        ds->send(buffer, len);
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