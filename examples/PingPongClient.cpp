#include <iostream>
#include <string>

#include "SocketLibFunction.h"
#include "WrapTCPService.h"

int main(int argc, char **argv)
{
    if (argc != 6)
    {
        fprintf(stderr, "Usage: <host> <port> <net work thread num> <session num> <packet size> \n");
        exit(-1);
    }

    std::string tmp(atoi(argv[5]), 'a');

    WrapServer::PTR server = std::make_shared<WrapServer>();

    server->startWorkThread(atoi(argv[3]));

    for (int i = 0; i < atoi(argv[4]); i++)
    {
        sock fd = ox_socket_connect(false, argv[1], atoi(argv[2]));
        server->addSession(fd, [&](TCPSession::PTR session){
            session->setDataCallback([](TCPSession::PTR session, const char* buffer, size_t len){
                session->send(buffer, len);
                return len;
            });
            session->send(tmp.c_str(), tmp.size());
        }, false, 1024 * 1024);
    }

    std::cin.get();
}