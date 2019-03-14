#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file
#include "catch.hpp"
#include <brynet/net/SyncConnector.h>
#include <brynet/net/ListenThread.h>

TEST_CASE("SyncConnector are computed", "[sync_connect]") {
    
    using namespace brynet::net;

    const std::string ip = "127.0.0.7";
    const auto port = 9999;

    {
        // 监听服务未开启
        auto socket = brynet::net::SyncConnectSocket({
                AsyncConnector::ConnectOptions::WithAddr(ip, port),
                AsyncConnector::ConnectOptions::WithTimeout(std::chrono::seconds(2))});
        REQUIRE(socket == nullptr);

        auto session = brynet::net::SyncConnectSession(nullptr,
            {
                AsyncConnector::ConnectOptions::WithAddr(ip, port),
                AsyncConnector::ConnectOptions::WithTimeout(std::chrono::seconds(2))
            }, 
            {});
        REQUIRE(session == nullptr);
    }

    {
        auto listenThread = brynet::net::ListenThread::Create(false, 
            "127.0.0.7", 
            port, 
            [](brynet::net::TcpSocket::Ptr) {
            });
        listenThread->startListen();

        auto socket = brynet::net::SyncConnectSocket({
                AsyncConnector::ConnectOptions::WithAddr(ip, port),
                AsyncConnector::ConnectOptions::WithTimeout(std::chrono::seconds(2)) });
        REQUIRE(socket != nullptr);
        
        // 没有service
        auto session = brynet::net::SyncConnectSession(nullptr,
            {
                AsyncConnector::ConnectOptions::WithAddr(ip, port),
                AsyncConnector::ConnectOptions::WithTimeout(std::chrono::seconds(2))
            },
            {});
        REQUIRE(session == nullptr);

        {
            auto service = brynet::net::TcpService::Create();
            service->startWorkerThread(1);
            session = brynet::net::SyncConnectSession(service,
                {
                    AsyncConnector::ConnectOptions::WithAddr(ip, port),
                    AsyncConnector::ConnectOptions::WithTimeout(std::chrono::seconds(2))
                },
                {
                    TcpService::AddSocketOption::WithMaxRecvBufferSize(1),
                });
            REQUIRE(session != nullptr);
        }

        {
            // 没有开启service工作线程
            auto service = brynet::net::TcpService::Create();
            session = brynet::net::SyncConnectSession(service,
                {
                    AsyncConnector::ConnectOptions::WithAddr(ip, port),
                    AsyncConnector::ConnectOptions::WithTimeout(std::chrono::seconds(2))
                },
                {
                    TcpService::AddSocketOption::WithMaxRecvBufferSize(1),
                });
            REQUIRE(session == nullptr);
        }
    }

    // 上个语句块的监听线程结束
    auto socket = brynet::net::SyncConnectSocket({
                AsyncConnector::ConnectOptions::WithAddr(ip, port),
                AsyncConnector::ConnectOptions::WithTimeout(std::chrono::seconds(2)) });
    REQUIRE(socket == nullptr);
}