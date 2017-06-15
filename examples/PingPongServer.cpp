#include <iostream>
#include <mutex>
#include <atomic>

#include "SocketLibFunction.h"
#include "EventLoop.h"
#include "WrapTCPService.h"

using namespace brynet;
using namespace brynet::net;

std::atomic_llong total_recv = ATOMIC_VAR_INIT(0);
std::atomic_llong total_client_num = ATOMIC_VAR_INIT(0);
std::atomic_llong total_packet_num = ATOMIC_VAR_INIT(0);

int main(int argc, char **argv)
{
    auto server = std::make_shared<WrapServer>();
    auto listenThread = ListenThread::Create();

    listenThread->startListen(false, "0.0.0.0", atoi(argv[1]), nullptr, nullptr, [=](int fd){
        std::cout << "enter" << std::endl;
        server->addSession(fd, [](TCPSession::PTR& session){
            total_client_num++;

            session->setDataCallback([](TCPSession::PTR& session, const char* buffer, size_t len){
                session->send(buffer, len);
                total_recv += len;
                total_packet_num++;
                return len;
            });

            session->setCloseCallback([](TCPSession::PTR& session){
                total_client_num--;
            });
        }, false, 1024*1024);
    });

    server->startWorkThread(atoi(argv[2]));

    EventLoop mainLoop;

    while (true)
    {
        mainLoop.loop(1000);
        std::cout << "total recv : " << (total_recv / 1024) / 1024 << " M /s, of client num:" << total_client_num << std::endl;
        std::cout << "packet num:" << total_packet_num << std::endl;
        total_packet_num = 0;
        total_recv = 0;
    }
}
