#include <iostream>
#include <mutex>
#include <atomic>

#include <brynet/net/SocketLibFunction.h>
#include <brynet/net/EventLoop.h>
#include <brynet/net/TCPService.h>
#include <brynet/net/ListenThread.h>
#include <brynet/net/Socket.h>

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

    auto server = TcpService::Create();
    auto listenThread = ListenThread::Create();

    listenThread->startListen(false, "0.0.0.0", atoi(argv[1]), [=](TcpSocket::PTR socket){
        socket->SocketNodelay();

        auto enterCallback = [](const DataSocket::PTR& session) {
            total_client_num++;

            session->setDataCallback([session](const char* buffer, size_t len) {
                session->send(buffer, len);
                TotalRecvSize += len;
                total_packet_num++;
                return len;
            });

            session->setDisConnectCallback([](const DataSocket::PTR& session) {
                total_client_num--;
            });
        };

        server->addDataSocket(std::move(socket),
            brynet::net::TcpService::AddSocketOption::WithEnterCallback(enterCallback),
            brynet::net::TcpService::AddSocketOption::WithMaxRecvBufferSize(1024 * 1024));
    });

    server->startWorkerThread(atoi(argv[2]));

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
