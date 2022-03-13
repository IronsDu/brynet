#include <brynet/base/AppStatus.hpp>
#include <brynet/net/AsyncConnector.hpp>
#include <brynet/net/TcpService.hpp>
#include <brynet/net/wrapper/ConnectionBuilder.hpp>
#include <iostream>
#include <string>

using namespace brynet;
using namespace brynet::net;

int main(int argc, char** argv)
{
    if (argc != 6)
    {
        fprintf(stderr, "Usage: <host> <port> <net work thread num> <session num> <packet size> \n");
        exit(-1);
    }

    std::string tmp(atoi(argv[5]), 'a');

    auto service = IOThreadTcpService::Create();
    service->startWorkerThread(atoi(argv[3]));

    auto connector = AsyncConnector::Create();
    connector->startWorkerThread();

    auto enterCallback = [tmp](const TcpConnection::Ptr& session) {
        session->setDataCallback([session](brynet::base::BasePacketReader& reader) {
            session->send(reader.begin(), reader.size());
            reader.consumeAll();
        });
        session->send(tmp);
    };

    auto failedCallback = []() {
        std::cout << "connect failed" << std::endl;
    };

    wrapper::ConnectionBuilder connectionBuilder;
    connectionBuilder
            .WithService(service)
            .WithConnector(connector)
            .WithMaxRecvBufferSize(1024 * 1024)
            .AddEnterCallback(enterCallback);

    const auto num = std::atoi(argv[4]);
    const auto ip = argv[1];
    const auto port = std::atoi(argv[2]);
    for (auto i = 0; i < num; i++)
    {
        try
        {
            connectionBuilder
                    .WithAddr(ip, port)
                    .WithTimeout(std::chrono::seconds(10))
                    .WithFailedCallback(failedCallback)
                    .AddSocketProcessCallback([](TcpSocket& socket) {
                        socket.setNodelay();
                    })
                    .asyncConnect();
        }
        catch (std::runtime_error& e)
        {
            std::cout << "error:" << e.what() << std::endl;
        }
    }

    while (true)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (brynet::base::app_kbhit())
        {
            break;
        }
    }

    return 0;
}
