#include <iostream>
#include <string>

#include "SocketLibFunction.h"
#include "WrapTCPService.h"

int onSessionMsg(TCPSession::PTR session, const char* buffer, size_t len)
{
    session->send(buffer, len);
    return len;
}

int main(int argc, char **argv)
{
    if (argc != 6)
    {
        fprintf(stderr, "Usage: <host> <port> <net work thread num> <session num> <packet size> \n");
        exit(-1);
    }

    std::string serverip = argv[1];
    int port_num = atoi(argv[3]);
    int thread_num = atoi(argv[2]);
    int num = atoi(argv[4]);
    int packet_len = atoi(argv[5]);

    std::string tmp(packet_len, 'a');

    WrapServer::PTR server = std::make_shared<WrapServer>();

    server->startWorkThread(thread_num);
    std::vector<TCPSession::PTR> slist;
    std::mutex slinuxLock;

    for (int i = 0; i < num; i++)
    {
        sock fd = ox_socket_connect(serverip.c_str(), port_num);
        server->addSession(fd, [&](TCPSession::PTR session){
            session->setDataCallback(onSessionMsg);
            session->send(tmp.c_str(), tmp.size());
            slinuxLock.lock();
            slist.push_back(session);
            slinuxLock.unlock();
        }, false, 1024 * 1024);
    }

    std::cin.get();

    slinuxLock.lock();
    for (auto& s : slist)
    {
        s->postClose();
    }
    slinuxLock.unlock();

    std::this_thread::sleep_for(std::chrono::seconds(5));
}