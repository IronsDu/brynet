#include <functional>
#include <time.h>
#include <thread>
#include <iostream>
#include <thread>
#include "packet.h"
#include "timer.h"

using namespace std;

#include "socketlibfunction.h"

#include "eventloop.h"
#include "datasocket.h"

int main(int argc, char **argv)
{
    int client_num = atoi(argv[1]);
    int packet_len = atoi(argv[2]);
    int port_num = atoi(argv[3]);

    ox_socket_init();

    EventLoop       mainLoop;

    std::thread* ts = new std::thread([&mainLoop, client_num, packet_len, port_num]{
        /*  ¿Í»§¶Ëeventloop*/
        EventLoop clientEventLoop;
        clientEventLoop.restoreThreadID();
        TimerMgr tm;

        char* senddata = (char*)malloc(packet_len);

        int64_t total_recv = 0;
        
        for (int i = 0; i < client_num; i++)
        {
            int client = ox_socket_connect("127.0.0.1", port_num);
            ox_socket_nodelay(client);

            DataSocket::PTR pClient = std::make_shared<DataSocket>(client);
            pClient->setEnterCallback([&](DataSocket::PTR ds){
                ds->send(senddata, packet_len);
                ds->setDataCallback([&total_recv](DataSocket::PTR ds, const char* buffer, int len){
                    ds->send(buffer, len);
                    total_recv += len;
                    return len;
                });
            });

            clientEventLoop.pushAsyncChannel(pClient);
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