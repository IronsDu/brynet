#include <functional>
#include <time.h>
#include <stdio.h>
#include <thread>
#include <iostream>
#include <assert.h>
#include <chrono>
#include <memory>
#include <thread>
#include "packet.h"

#include "SocketLibFunction.h"

#include "EventLoop.h"
#include "DataSocket.h"
#include "timer.h"

using namespace std;

int main(int argc, char** argv)
{
    double total_recv_len = 0;
    double  packet_num = 0;

    int client_num = atoi(argv[1]);
    int packet_len = atoi(argv[2]);
    int port_num = atoi(argv[3]);

    ox_socket_init();

    EventLoop       mainLoop;

    /*  客户端IO线程   */

    std::thread* ts = new std::thread([&mainLoop, client_num, packet_len, port_num]{
        printf("start one client thread \n");
        /*  客户端eventloop*/
        EventLoop clientEventLoop;

        char* senddata = (char*)malloc(packet_len);

        /*  消息包大小定义 */

        int64_t total_recv = 0;
        TimerMgr tm;
        for (int i = 0; i < client_num; i++)
        {
            int client = ox_socket_connect("127.0.0.1", port_num);
            ox_socket_nodelay(client);

            DataSocket::PTR pClient = new DataSocket(client);
            pClient->setEnterCallback([&](DataSocket::PTR ds){

                LongPacket sp;
                sp.setOP(1);
                sp.writeINT64((int64_t)ds);
                sp.writeBinary(senddata, packet_len);
                sp.end();

                for (int i = 0; i < 1; ++i)
                {
                    ds->send(sp.getData(), sp.getLen());
                }

                /*  可以放入消息队列，然后唤醒它主线程的eventloop，然后主线程通过消息队列去获取*/
                ds->setDataCallback([&total_recv](DataSocket::PTR ds, const char* buffer, int len){
                    const char* parse_str = buffer;
                    int total_proc_len = 0;
                    int left_len = len;

                    while (true)
                    {
                        bool flag = false;
                        if (left_len >= sizeof(sizeof(uint16_t) + sizeof(uint16_t)))
                        {
                            ReadPacket rp(parse_str, left_len);
                            uint16_t packet_len = rp.readINT16();
                            if (left_len >= packet_len && packet_len >= (sizeof(uint16_t) + sizeof(uint16_t)))
                            {
                                total_recv += packet_len;

                                ReadPacket rp(parse_str, packet_len);
                                rp.readINT16();
                                rp.readINT16();
                                int64_t addr = rp.readINT64();

                                if (addr == (int64_t)(ds))
                                {
                                    ds->send(parse_str, packet_len);
                                }

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

                ds->setDisConnectCallback([](DataSocket::PTR arg){
                    delete arg;
                });
            });

            clientEventLoop.pushAsyncProc([&, pClient](){
                if (!pClient->onEnterEventLoop(&clientEventLoop))
                {
                    delete pClient;
                }
            });
        }

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

    ts->join();
}