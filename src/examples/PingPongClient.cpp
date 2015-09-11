#include <iostream>
#include <string>

#include "SocketLibFunction.h"
#include "WrapTCPService.h"

void onSessionClose(TCPSession::PTR session)
{
}

int onSessionMsg(TCPSession::PTR session, const char* buffer, int len)
{
    session->send(buffer, len);
    return len;
}

int main(int argc, char **argv)
{
    int thread_num = atoi(argv[1]);
    int port_num = atoi(argv[2]);
    int num = atoi(argv[3]);
    int packet_len = atoi(argv[4]);

    std::string tmp(packet_len, 'a');

    WrapServer::PTR server = std::make_shared<WrapServer>();

    server->startWorkThread(thread_num);

    for (int i = 0; i < num; i++)
    {
        sock fd = ox_socket_connect("127.0.0.1", port_num);
        server->addSession(fd, [&](TCPSession::PTR session){
            session->setCloseCallback(onSessionClose);
            session->setDataCallback(onSessionMsg);
            session->send(tmp.c_str(), tmp.size());
        });
    }

    std::cin.get();
}