#include <atomic>
#include <brynet/base/AppStatus.hpp>
#include <brynet/net/EventLoop.hpp>
#include <brynet/net/TcpService.hpp>
#include <brynet/net/wrapper/ServiceBuilder.hpp>
#include <iostream>
#include <mutex>

using namespace brynet;
using namespace brynet::net;

std::atomic_llong TotalRecvSize = ATOMIC_VAR_INIT(0);
std::atomic_llong total_client_num = ATOMIC_VAR_INIT(0);
std::atomic_llong total_packet_num = ATOMIC_VAR_INIT(0);

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage: <listen port>\n");
        exit(-1);
    }

    auto enterCallback = [](const TcpConnection::Ptr& session) {
        total_client_num++;

        session->setDataCallback([session](brynet::base::BasePacketReader& reader) {
            session->send(reader.begin(), reader.size());
            TotalRecvSize += reader.size();
            total_packet_num++;
            reader.consumeAll();
        });

        session->setDisConnectCallback([](const TcpConnection::Ptr& session) {
            (void) session;
            total_client_num--;
        });
    };

    EventLoop::Ptr mainLoop = std::make_shared<EventLoop>();
    mainLoop->bindCurrentThread();

    auto service = EventLoopTcpService::Create(mainLoop);

    wrapper::ListenerBuilder listener;
    listener.WithService(service)
            .AddSocketProcess({[](TcpSocket& socket) {
                socket.setNodelay();
            }})
            .WithMaxRecvBufferSize(1024)
            .AddEnterCallback(enterCallback)
            .WithAddr(false, "0.0.0.0", atoi(argv[1]))
            .asyncRun();

    mainLoop->runIntervalTimer(std::chrono::seconds(1), [&]() {
        if (TotalRecvSize / 1024 == 0)
        {
            std::cout << "total recv : " << TotalRecvSize << " bytes/s, of client num:" << total_client_num << std::endl;
        }
        else if ((TotalRecvSize / 1024) / 1024 == 0)
        {
            std::cout << "total recv : " << TotalRecvSize / 1024 << " K/s, of client num:" << total_client_num << std::endl;
        }
        else
        {
            std::cout << "total recv : " << (TotalRecvSize / 1024) / 1024 << " M/s, of client num:" << total_client_num << std::endl;
        }

        std::cout << "packet num:" << total_packet_num << std::endl;
        total_packet_num = 0;
        TotalRecvSize = 0;
    });

    while (true)
    {
        mainLoop->loop(1000);

        if (brynet::base::app_kbhit())
        {
            break;
        }
    }

    return 0;
}
