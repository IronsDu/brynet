#include <iostream>
#include <mutex>
#include <atomic>

#include "SocketLibFunction.h"
#include "EventLoop.h"
#include "WrapTCPService.h"

std::atomic_llong total_recv = ATOMIC_VAR_INIT(0);
std::atomic_llong total_client_num = ATOMIC_VAR_INIT(0);

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage:  <listen port> <net work thread num>\n");
        exit(-1);
    }

    WrapServer::PTR server = std::make_shared<WrapServer>();
    ListenThread::PTR listenThread = std::make_shared<ListenThread>();

    listenThread->startListen(false, "127.0.0.1", atoi(argv[1]), nullptr, nullptr, [=](int fd){
        server->addSession(fd, [](TCPSession::PTR session){
            total_client_num++;

            session->setDataCallback([](TCPSession::PTR session, const char* buffer, size_t len){
                session->send(buffer, len);
                total_recv += len;
                return len;
            });

            session->setCloseCallback([](TCPSession::PTR session){
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
        total_recv = 0;
    }
}
