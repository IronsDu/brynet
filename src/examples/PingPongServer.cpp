#include <iostream>
#include <mutex>
#include "SocketLibFunction.h"
#include "EventLoop.h"
#include "WrapTCPService.h"

std::mutex g_mutex;
int total_recv = 0;
int total_client_num = 0;

void onSessionClose(TCPSession::PTR session)
{
    g_mutex.lock();
    total_client_num--;
    g_mutex.unlock();
}

int onSessionMsg(TCPSession::PTR session, const char* buffer, int len)
{
    session->send(buffer, len);
    g_mutex.lock();
    total_recv += len;
    g_mutex.unlock();
    return len;
}

int main(int argc, char **argv)
{
    int thread_num = atoi(argv[1]);
    int port_num = atoi(argv[2]);

    WrapServer::PTR server = std::make_shared<WrapServer>();
    ListenThread::PTR listenThread = std::make_shared<ListenThread>();

    listenThread->startListen(port_num, nullptr, nullptr, [=](int fd){
        server->addSession(fd, [](TCPSession::PTR session){
            session->setCloseCallback(onSessionClose);
            session->setDataCallback(onSessionMsg);

            g_mutex.lock();
            total_client_num++;
            g_mutex.unlock();
        }, false);
    });

    server->startWorkThread(thread_num);

    EventLoop mainLoop;

    while (true)
    {
        mainLoop.loop(1000);
        g_mutex.lock();
        std::cout << "total recv : " << (total_recv / 1024) / 1024 << " M /s, of client num:" << total_client_num << std::endl;
        total_recv = 0;
        g_mutex.unlock();
    }
}