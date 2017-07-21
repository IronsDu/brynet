#include <iostream>
#include <mutex>
#include <atomic>

#include "SocketLibFunction.h"
#include "EventLoop.h"
#include "WrapTCPService.h"

using namespace brynet;
using namespace brynet::net;

std::atomic_llong TotalRecvSize = ATOMIC_VAR_INIT(0);
std::atomic_llong total_client_num = ATOMIC_VAR_INIT(0);
std::atomic_llong total_packet_num = ATOMIC_VAR_INIT(0);

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage: <listen port> <net work thread num>\n");
        exit(-1);
    }

    auto server = std::make_shared<WrapTcpService>();
    auto listenThread = ListenThread::Create();

    listenThread->startListen(false, "0.0.0.0", atoi(argv[1]), nullptr, nullptr, [=](int fd){
        server->addSession(fd, [](const TCPSession::PTR& session){
            total_client_num++;

            session->setDataCallback([](const TCPSession::PTR& session, const char* buffer, size_t len){
                session->send(buffer, len);
                TotalRecvSize += len;
                total_packet_num++;
                return len;
            });

            session->setCloseCallback([](const TCPSession::PTR& session){
                total_client_num--;
            });
        }, false, 1024*1024);
    });

    server->startWorkThread(atoi(argv[2]));

    EventLoop mainLoop;
    while (true)
    {
        mainLoop.loop(1000);
        std::cout << "total recv : " << (TotalRecvSize / 1024) / 1024 << " M /s, of client num:" << total_client_num << std::endl;
        std::cout << "packet num:" << total_packet_num << std::endl;
        total_packet_num = 0;
        TotalRecvSize = 0;
    }
}
