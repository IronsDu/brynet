#include <functional>
#include <time.h>
#include <stdio.h>
#include <thread>
#include <iostream>
#include <assert.h>
#include <chrono>
#include <memory>
#include <thread>


#include "socketlibfunction.h"

#include "eventloop.h"
#include "datasocket.h"
#include "timer.h"

using namespace std;

void SSL_init()
{
#ifdef USE_OPENSSL
    /* SSL 库初始化 */
    SSL_library_init();
    /* 载入所有 SSL 算法 */
    OpenSSL_add_all_algorithms();
    /* 载入所有 SSL 错误消息 */
    SSL_load_error_strings();
#endif
}

typedef std::shared_ptr<DataSocket> SHARED_DATASOCKET;

static void fuck_send(TimerMgr* tm, std::weak_ptr<SHARED_DATASOCKET> ds, const char* buffer, int len)
{
    std::shared_ptr<SHARED_DATASOCKET> tmp = ds.lock();
    if (tmp)
    {
        tmp->get()->send(buffer, len);
        tm->AddTimer(60, fuck_send, tm, ds, buffer, len);
    }
    else
    {
        cout << "haha" << endl;
    }
}

__declspec(thread) int tls_i = 1;

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
    if (packet_len <= (sizeof(int16_t) + sizeof(int16_t)))
    {
        packet_len = 4;
    }

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
            clientEventLoop.restoreThreadID();

            /*  消息包大小定义 */
            char* senddata = nullptr;
            if (packet_len > 0)
            {
                senddata = (char*)malloc(packet_len);
                ((int16_t*)senddata)[0] = packet_len;
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
                    
                    shared_ptr<SHARED_DATASOCKET> tmp = std::make_shared<SHARED_DATASOCKET>();
                    tmp->reset(ds);
                    if (senddata != nullptr)
                    {
                        for (int i = 0; i < 100; ++i)
                        {
                            ds->send(senddata, packet_len);
                        }
                        //fuck_send(&tm, tmp, senddata, packet_len);
                    }

                    /*  可以放入消息队列，然后唤醒它主线程的eventloop，然后主线程通过消息队列去获取*/
                    ds->setDataHandle([&total_recv](DataSocket* ds, const char* buffer, int len){
                        const char* parse_str = buffer;
                        int total_proc_len = 0;
                        int left_len = len;

                        while (true)
                        {
                            bool flag = false;
                            if (left_len >= sizeof(sizeof(uint16_t) + sizeof(uint16_t)))
                            {
                                uint16_t packet_len = (*(uint16_t*)parse_str);
                                if (left_len >= packet_len && packet_len >= (sizeof(uint16_t) + sizeof(uint16_t)))
                                {
                                    ds->send(parse_str, packet_len);

                                    total_proc_len += packet_len;
                                    parse_str += packet_len;
                                    left_len -= packet_len;
                                    flag = true;
                                }
                            }

                            if (!flag)
                            {
                                break;
                            }
                        }

                        return total_proc_len;
                    });

                    ds->setDisConnectHandle([tmp](DataSocket* arg){
                        tmp->reset();
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