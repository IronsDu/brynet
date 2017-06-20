#include <iostream>
#include <string>

#include "SocketLibFunction.h"
#include "WrapTCPService.h"
#include "Connector.h"

using namespace brynet;
using namespace brynet::net;

int main(int argc, char **argv)
{
    if (argc != 6)
    {
        fprintf(stderr, "Usage: <host> <port> <net work thread num> <session num> <packet size> \n");
        exit(-1);
    }

    std::string tmp(atoi(argv[5]), 'a');

    auto server = std::make_shared<WrapServer>();
    server->startWorkThread(atoi(argv[3]));

    auto connector = ThreadConnector::Create();
    connector->startThread([server, tmp](sock fd, const std::any& ud) {
        if (fd != -1)
        {
            server->addSession(fd, [tmp](TCPSession::PTR& session) {
                session->setDataCallback([](TCPSession::PTR& session, const char* buffer, size_t len) {
                    session->send(buffer, len);
                    return len;
                });
                session->send(tmp.c_str(), tmp.size());
            }, false, 1024 * 1024);
        }
    });

    for (auto i = 0; i < atoi(argv[4]); i++)
    {
        connector->asyncConnect(argv[1], atoi(argv[2]), 10000, nullptr);
    }

    std::cin.get();
}