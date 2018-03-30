#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file
#include "catch.hpp"
#include <brynet/net/SyncConnector.h>
#include <brynet/net/ListenThread.h>

TEST_CASE("SyncConnector are computed", "[sync_connect]") {
    const std::string ip = "127.0.0.7";
    const auto port = 9999;

    {
        auto socket = brynet::net::SyncConnectSocket(ip, port, std::chrono::seconds(2));
        REQUIRE(socket == nullptr);

        auto session = brynet::net::SyncConnectSession(ip, port, std::chrono::seconds(2));
        REQUIRE(session == nullptr);
    }

    auto listenThread = brynet::net::ListenThread::Create();
    listenThread->startListen(false, "127.0.0.7", port, [](brynet::net::TcpSocket::PTR) {
    });

    {
        auto socket = brynet::net::SyncConnectSocket(ip, port, std::chrono::seconds(2));
        REQUIRE(socket != nullptr);

        auto session = brynet::net::SyncConnectSession(ip, port, std::chrono::seconds(2));
        REQUIRE(session != nullptr);
    }

    listenThread->stopListen();
}