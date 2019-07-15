#include <iostream>
#include <string>

#include <brynet/net/SocketLibFunction.h>
#include <brynet/net/TCPService.h>
#include <brynet/net/Connector.h>
#include <brynet/net/Wrapper.h>

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

    auto service = TcpService::Create();
    service->startWorkerThread(atoi(argv[3]));

    auto connector = AsyncConnector::Create();
    connector->startWorkerThread();

    auto enterCallback = [tmp](const TcpConnection::Ptr& session) {
        session->setDataCallback([session](const char* buffer, size_t len) {
                session->send(buffer, len);
                return len;
            });
        session->send(tmp.c_str(), tmp.size());
    };

    auto failedCallback = []() {
        std::cout << "connect failed" << std::endl;
    };

    wrapper::ConnectionBuilder connectionBuilder;
    connectionBuilder.configureService(service)
        .configureConnector(connector)
        .configureConnectionOptions({
            brynet::net::TcpService::AddSocketOption::AddEnterCallback(enterCallback),
            brynet::net::TcpService::AddSocketOption::WithMaxRecvBufferSize(1024 * 1024)
        });

    const auto num = std::atoi(argv[4]);
    const auto ip = argv[1];
    const auto port = std::atoi(argv[2]);
    for (auto i = 0; i < num; i++)
    {
        try
        {
            connectionBuilder.configureConnectOptions({
                    AsyncConnector::ConnectOptions::WithAddr(ip, port),
                    AsyncConnector::ConnectOptions::WithTimeout(std::chrono::seconds(10)),
                    AsyncConnector::ConnectOptions::WithFailedCallback(failedCallback),
                    AsyncConnector::ConnectOptions::AddProcessTcpSocketCallback([](TcpSocket& socket) {
                        socket.setNodelay();
                    })
                })
                .asyncConnect();
        }
        catch (std::runtime_error& e)
        {
            std::cout << "error:" << e.what() << std::endl;
        }
    }

    std::cin.get();
}
