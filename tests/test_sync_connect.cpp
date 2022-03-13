#define CATCH_CONFIG_MAIN// This tells Catch to provide a main() - only do this in one cpp file
#include <brynet/net/ListenThread.hpp>
#include <brynet/net/wrapper/ConnectionBuilder.hpp>
#include <brynet/net/wrapper/ServiceBuilder.hpp>

#include "catch.hpp"

TEST_CASE("SyncConnector are computed", "[sync_connect]")
{

    using namespace brynet::net;

    const std::string ip = "127.0.0.1";
    const auto port = 9999;

    {
            {}}
    // listen service not open
    {
            {auto connector = AsyncConnector::Create();
    connector->startWorkerThread();
    wrapper::SocketConnectBuilder connectBuilder;
    auto socket = connectBuilder
                          .WithConnector(connector)
                          .WithTimeout(std::chrono::seconds(2))
                          .WithAddr(ip, port)
                          .syncConnect();

    REQUIRE(socket == nullptr);
}

{
    auto connector = AsyncConnector::Create();
    connector->startWorkerThread();
    auto service = IOThreadTcpService::Create();
    service->startWorkerThread(1);

    wrapper::ConnectionBuilder connectionBuilder;
    auto session = connectionBuilder
                           .WithService(service)
                           .WithConnector(connector)
                           .WithTimeout(std::chrono::seconds(2))
                           .WithAddr(ip, port)
                           .syncConnect();

    REQUIRE(session == nullptr);
}
}

// use ListenerBuilder for open listen
{
    auto service = IOThreadTcpService::Create();
    service->startWorkerThread(1);
    wrapper::ListenerBuilder listenerBuilder;
    listenerBuilder.WithService(service)
            .WithMaxRecvBufferSize(10)
            .WithAddr(false, ip, port)
            .asyncRun();

    {
        auto connector = AsyncConnector::Create();
        connector->startWorkerThread();
        wrapper::SocketConnectBuilder connectBuilder;
        auto socket = connectBuilder
                              .WithConnector(connector)
                              .WithTimeout(std::chrono::seconds(2))
                              .WithAddr(ip, port)
                              .syncConnect();

        REQUIRE(socket != nullptr);
    }
}

{
    auto connector = AsyncConnector::Create();
    connector->startWorkerThread();
    wrapper::SocketConnectBuilder connectBuilder;
    auto socket = connectBuilder
                          .WithConnector(connector)
                          .WithTimeout(std::chrono::seconds(2))
                          .WithAddr(ip, port)
                          .syncConnect();

    REQUIRE(socket == nullptr);
}

// open listen thread
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
                              .WithConnector(connector)
                              .WithTimeout(std::chrono::seconds(2))
                              .WithAddr(ip, port)
                              .syncConnect();

        REQUIRE(socket != nullptr);
    }

    // Tcp Service start io work thread
    {
        auto connector = AsyncConnector::Create();
        connector->startWorkerThread();
        auto service = IOThreadTcpService::Create();
        service->startWorkerThread(1);

        wrapper::ConnectionBuilder connectionBuilder;
        auto session = connectionBuilder
                               .WithService(service)
                               .WithConnector(connector)
                               .WithTimeout(std::chrono::seconds(2))
                               .WithAddr(ip, port)
                               .syncConnect();

        REQUIRE(session != nullptr);
    }

    // Tcp Service not open io work thread
    {
        auto connector = AsyncConnector::Create();
        connector->startWorkerThread();
        auto service = IOThreadTcpService::Create();

        wrapper::ConnectionBuilder connectionBuilder;
        auto session = connectionBuilder
                               .WithService(service)
                               .WithConnector(connector)
                               .WithTimeout(std::chrono::seconds(2))
                               .WithAddr(ip, port)
                               .syncConnect();

        REQUIRE(session == nullptr);
    }

    //  use EventLoop for Tcp Service
    {
        auto connector = AsyncConnector::Create();
        connector->startWorkerThread();
        auto eventLoop = std::make_shared<EventLoop>();
        eventLoop->bindCurrentThread();
        auto service = EventLoopTcpService::Create(eventLoop);

        wrapper::ConnectionBuilder connectionBuilder;
        auto session = connectionBuilder
                               .WithService(service)
                               .WithConnector(connector)
                               .WithTimeout(std::chrono::seconds(2))
                               .WithAddr(ip, port)
                               .syncConnect();

        REQUIRE(session != nullptr);
    }
}

// listen thread exit
{
    auto connector = AsyncConnector::Create();
    connector->startWorkerThread();
    wrapper::SocketConnectBuilder connectBuilder;
    auto socket = connectBuilder
                          .WithConnector(connector)
                          .WithTimeout(std::chrono::seconds(2))
                          .WithAddr(ip, port)
                          .syncConnect();

    REQUIRE(socket == nullptr);
}
}
