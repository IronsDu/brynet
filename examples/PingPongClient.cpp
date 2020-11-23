#include <iostream>
#include <string>

#include <brynet/net/TcpService.hpp>
#include <brynet/net/AsyncConnector.hpp>
#include <brynet/net/wrapper/ConnectionBuilder.hpp>
#include <brynet/base/AppStatus.hpp>

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
        session->setDataCallback([session](brynet::base::BasePacketReader& reader) {
                session->send(reader.begin(), reader.size());
                reader.consumeAll();
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
            brynet::net::AddSocketOption::AddEnterCallback(enterCallback),
            brynet::net::AddSocketOption::WithMaxRecvBufferSize(1024 * 1024)
        });

    const auto num = std::atoi(argv[4]);
    const auto ip = argv[1];
    const auto port = std::atoi(argv[2]);
    for (auto i = 0; i < num; i++)
    {
        try
        {
            connectionBuilder.configureConnectOptions({
                    ConnectOption::WithAddr(ip, port),
                    ConnectOption::WithTimeout(std::chrono::seconds(10)),
                    ConnectOption::WithFailedCallback(failedCallback),
                    ConnectOption::AddProcessTcpSocketCallback([](TcpSocket& socket) {
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

    while(true)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (brynet::base::app_kbhit())
        {
            break;
        }
    }

    return 0;
}
