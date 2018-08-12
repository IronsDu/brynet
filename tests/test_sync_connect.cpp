#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file
#include "catch.hpp"
#include <brynet/net/SyncConnector.h>
#include <brynet/net/ListenThread.h>

TEST_CASE("SyncConnector are computed", "[sync_connect]") {
    
    const std::string ip = "127.0.0.7";
    const auto port = 9999;

    {
        // 监听服务未开启
        auto socket = brynet::net::SyncConnectSocket(ip, port, std::chrono::seconds(2));
        REQUIRE(socket == nullptr);

        auto session = brynet::net::SyncConnectSession(ip, port, std::chrono::seconds(2), nullptr, {});
        REQUIRE(session == nullptr);
    }

    {
        auto listenThread = brynet::net::ListenThread::Create();
        listenThread->startListen(false, "127.0.0.7", port, [](brynet::net::TcpSocket::PTR) {
        });

        auto socket = brynet::net::SyncConnectSocket(ip, port, std::chrono::seconds(10));
        REQUIRE(socket != nullptr);
        
        // 没有service
        auto session = brynet::net::SyncConnectSession(ip, port, std::chrono::seconds(2), nullptr, {});
        REQUIRE(session == nullptr);

        {
            std::cout << "start 35" << std::endl;
            auto service = brynet::net::TcpService::Create();
            service->startWorkerThread(1);
            session = brynet::net::SyncConnectSession(ip, 
                port, 
                std::chrono::seconds(2), 
                service, 
                {brynet::net::TcpService::AddSocketOption::WithMaxRecvBufferSize(1)});
            REQUIRE(session != nullptr);
            std::cout << "end 35" << std::endl;
        }

        {
            // 没有开启service工作线程
            auto service = brynet::net::TcpService::Create();
            session = brynet::net::SyncConnectSession(ip, 
                port, 
                std::chrono::seconds(2), 
                service, 
                { brynet::net::TcpService::AddSocketOption::WithMaxRecvBufferSize(1) });
            REQUIRE(session == nullptr);
        }
    }

    // 上个语句块的监听线程结束
    auto socket = brynet::net::SyncConnectSocket(ip, port, std::chrono::seconds(2));
    REQUIRE(socket == nullptr);
}