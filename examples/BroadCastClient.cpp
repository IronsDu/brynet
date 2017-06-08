#include <functional>
#include <time.h>
#include <stdio.h>
#include <thread>
#include <iostream>
#include <assert.h>
#include <chrono>
#include <memory>
#include <thread>
#include <atomic>

#include "packet.h"

#include "SocketLibFunction.h"

#include "EventLoop.h"
#include "DataSocket.h"
#include "Timer.h"

using namespace std;
using namespace brynet;
using namespace brynet::net;

int main(int argc, char** argv)
{
    if (argc != 5)
    {
        fprintf(stderr, "Usage: <server ip> <server port> <session num> <packet size>\n");
        exit(-1);
    }

    std::string ip = argv[1];
    int port_num = atoi(argv[2]);
    int client_num = atoi(argv[3]);
    int packet_len = atoi(argv[4]);

    ox_socket_init();

    EventLoop       mainLoop;

    /*  客户端IO线程   */

    std::thread* ts = new std::thread([&mainLoop, client_num, packet_len, port_num, ip]{
        printf("start one client thread \n");
        /*  客户端eventloop*/
        auto clientEventLoop = std::make_shared<EventLoop>();

        char* senddata = (char*)malloc(packet_len);

        /*  消息包大小定义 */
        atomic_llong  packet_num = ATOMIC_VAR_INIT(0);
        atomic_llong total_recv = ATOMIC_VAR_INIT(0);
        TimerMgr tm;
        for (int i = 0; i < client_num; i++)
        {
            int client = ox_socket_connect(false, ip.c_str(), port_num);
            ox_socket_nodelay(client);

            DataSocket::PTR pClient = new DataSocket(client, 1024 * 1024);
            pClient->setEnterCallback([&](DataSocket::PTR ds){

                std::shared_ptr<BigPacket> sp = std::make_shared<BigPacket>(1);
                sp->writeINT64((int64_t)ds);
                sp->writeBinary(senddata, packet_len);

                for (int i = 0; i < 1; ++i)
                {
                    ds->send(sp->getData(), sp->getLen());
                }

                /*  可以放入消息队列，然后唤醒它主线程的eventloop，然后主线程通过消息队列去获取*/
                ds->setDataCallback([&total_recv, &packet_num](DataSocket::PTR ds, const char* buffer, size_t len){
                    const char* parse_str = buffer;
                    int total_proc_len = 0;
                    size_t left_len = len;

                    while (true)
                    {
                        bool flag = false;
                        if (left_len >= PACKET_HEAD_LEN)
                        {
                            ReadPacket rp(parse_str, left_len);
                            PACKET_LEN_TYPE packet_len = rp.readPacketLen();
                            if (left_len >= packet_len && packet_len >= PACKET_HEAD_LEN)
                            {
                                total_recv += packet_len;
                                packet_num++;

                                ReadPacket rp(parse_str, packet_len);
                                rp.readPacketLen();
                                rp.readOP();
                                int64_t addr = rp.readINT64();

                                if (addr == (int64_t)(ds))
                                {
                                    ds->send(parse_str, packet_len);
                                }

                                total_proc_len += packet_len;
                                parse_str += packet_len;
                                left_len -= packet_len;
                                flag = true;
                                rp.skipAll();
                            }
                            rp.skipAll();
                        }

                        if (!flag)
                        {
                            break;
                        }
                    }

                    return total_proc_len;
                });

                ds->setDisConnectCallback([](DataSocket::PTR arg){
                    delete arg;
                });
            });

            clientEventLoop->pushAsyncProc([&, pClient](){
                if (!pClient->onEnterEventLoop(clientEventLoop))
                {
                    delete pClient;
                }
            });
        }

        int64_t now = ox_getnowtime();
        while (true)
        {
            clientEventLoop->loop(tm.nearEndMs());
            tm.schedule();
            if ((ox_getnowtime() - now) >= 1000)
            {
                cout << "total recv:" << (total_recv / 1024) / 1024 << " M /s" << " , num " << packet_num << endl;
                now = ox_getnowtime();
                total_recv = 0;
                packet_num = 0;
            }
        }
    });

    ts->join();
}
