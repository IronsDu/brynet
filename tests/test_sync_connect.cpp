#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file
#include "catch.hpp"
#include <brynet/net/wrapper/ConnectionBuilder.hpp>
#include <brynet/net/wrapper/ServiceBuilder.hpp>
#include <brynet/net/ListenThread.hpp>

TEST_CASE("SyncConnector are computed", "[sync_connect]") {
    
    using namespace brynet::net;

    const std::string ip = "127.0.0.1";
    const auto port = 9999;

    // 监听服务未开启
    {
        {
            auto connector = AsyncConnector::Create();
            connector->startWorkerThread();
            wrapper::SocketConnectBuilder connectBuilder;
            auto socket = connectBuilder
                .configureConnector(connector)
                .configureConnectOptions({
                    ConnectOption::WithTimeout(std::chrono::seconds(2)),
                    ConnectOption::WithAddr(ip, port)
                    })
                .syncConnect();

            REQUIRE(socket == nullptr);
        }

        {
            auto connector = AsyncConnector::Create();
            connector->startWorkerThread();
            auto service = TcpService::Create();
            service->startWorkerThread(1);

            wrapper::ConnectionBuilder connectionBuilder;
            auto session = connectionBuilder
                .configureService(service)
                .configureConnector(connector)
                .configureConnectOptions({
                    ConnectOption::WithTimeout(std::chrono::seconds(2)),
                    ConnectOption::WithAddr(ip, port)
                })
                .syncConnect();

            REQUIRE(session == nullptr);
        }
    }

    // 使用ListenerBuilder开启监听
    {
        auto service = TcpService::Create();
        service->startWorkerThread(1);
        wrapper::ListenerBuilder listenerBuilder;
        listenerBuilder.configureService(service)
            .configureConnectionOptions({
                AddSocketOption::WithMaxRecvBufferSize(10)
            })
            .configureSocketOptions({})
            .configureListen([=](wrapper::BuildListenConfig config) {
                config.setAddr(false, ip, port);
            })
            .asyncRun();

            {
                auto connector = AsyncConnector::Create();
                connector->startWorkerThread();
                wrapper::SocketConnectBuilder connectBuilder;
                auto socket = connectBuilder
                    .configureConnector(connector)
                    .configureConnectOptions({
                        ConnectOption::WithTimeout(std::chrono::seconds(2)),
                        ConnectOption::WithAddr(ip, port)
                        })
                    .syncConnect();

                REQUIRE(socket != nullptr);
            }
    }

    {
        auto connector = AsyncConnector::Create();
        connector->startWorkerThread();
        wrapper::SocketConnectBuilder connectBuilder;
        auto socket = connectBuilder
            .configureConnector(connector)
            .configureConnectOptions({
                ConnectOption::WithTimeout(std::chrono::seconds(2)),
                ConnectOption::WithAddr(ip, port)
                })
            .syncConnect();

        REQUIRE(socket == nullptr);
    }

    // 开启监听服务
    {
        auto listenThread = brynet::net::ListenThread::Create(false, 
            ip, 
            port, 
            [](brynet::net::TcpSocket::Ptr) {
            });
        listenThread->startListen();

        {
            auto connector = AsyncConnector::Create();
            connector->startWorkerThread();
            wrapper::SocketConnectBuilder connectBuilder;
            auto socket = connectBuilder
                .configureConnector(connector)
                .configureConnectOptions({
                    ConnectOption::WithTimeout(std::chrono::seconds(2)),
                    ConnectOption::WithAddr(ip, port)
                })
                .syncConnect();

            REQUIRE(socket != nullptr);
        }
        
        // Tcp Service 开启工作线程
        {
            auto connector = AsyncConnector::Create();
            connector->startWorkerThread();
            auto service = TcpService::Create();
            service->startWorkerThread(1);

            wrapper::ConnectionBuilder connectionBuilder;
            auto session = connectionBuilder
                .configureService(service)
                .configureConnector(connector)
                .configureConnectOptions({
                    ConnectOption::WithTimeout(std::chrono::seconds(2)),
                    ConnectOption::WithAddr(ip, port)
                })
                .syncConnect();

            REQUIRE(session != nullptr);
        }

        // Tcp Service 没开启工作线程
        {
            auto connector = AsyncConnector::Create();
            connector->startWorkerThread();
            auto service = TcpService::Create();

            wrapper::ConnectionBuilder connectionBuilder;
            auto session = connectionBuilder
                .configureService(service)
                .configureConnector(connector)
                .configureConnectOptions({
                    ConnectOption::WithTimeout(std::chrono::seconds(2)),
                    ConnectOption::WithAddr(ip, port)
                })
                .syncConnect();

            REQUIRE(session == nullptr);
        }
    }

    // 上个语句块的监听线程结束
    {
        auto connector = AsyncConnector::Create();
        connector->startWorkerThread();
        wrapper::SocketConnectBuilder connectBuilder;
        auto socket = connectBuilder
            .configureConnector(connector)
            .configureConnectOptions({
                ConnectOption::WithTimeout(std::chrono::seconds(2)),
                ConnectOption::WithAddr(ip, port)
            })
            .syncConnect();

        REQUIRE(socket == nullptr);
    }
}