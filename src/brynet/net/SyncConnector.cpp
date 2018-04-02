#include <future>
#include <memory>
#include <chrono>

#include <brynet/net/SyncConnector.h>

brynet::net::TcpSocket::PTR brynet::net::SyncConnectSocket(std::string ip,
    int port,
    std::chrono::milliseconds timeout,
    brynet::net::AsyncConnector::PTR asyncConnector)
{
    if (asyncConnector == nullptr)
    {
        asyncConnector = AsyncConnector::Create();
        asyncConnector->startWorkerThread();
    }

    auto socketPromise = std::make_shared<std::promise<TcpSocket::PTR>>();
    asyncConnector->asyncConnect(
        ip,
        port,
        timeout,
        [socketPromise](TcpSocket::PTR socket) {
        socketPromise->set_value(std::move(socket));
    }, [socketPromise]() {
        socketPromise->set_value(nullptr);
    });

    return socketPromise->get_future().get();
}

brynet::net::TCPSession::PTR brynet::net::SyncConnectSession(std::string ip,
    int port,
    std::chrono::milliseconds timeout,
    brynet::net::WrapTcpService::PTR service,
    brynet::net::AsyncConnector::PTR asyncConnector)
{
    if (service == nullptr)
    {
        return nullptr;
    }

    if (asyncConnector == nullptr)
    {
        asyncConnector = AsyncConnector::Create();
        asyncConnector->startWorkerThread();
    }

    auto sessionPromise = std::make_shared<std::promise<TCPSession::PTR>>();
    asyncConnector->asyncConnect(
        ip,
        port,
        timeout,
        [service, sessionPromise](TcpSocket::PTR socket) {
        socket->SocketNodelay();
        service->addSession(
            std::move(socket),
            [sessionPromise](const TCPSession::PTR& session) {
            sessionPromise->set_value(session);
        },
            false,
            nullptr,
            1024 * 1024);
    }, [sessionPromise]() {
        sessionPromise->set_value(nullptr);
    });

    auto future = sessionPromise->get_future();
    if (future.wait_for(timeout) != std::future_status::ready)
    {
        return nullptr;
    }
    
    return future.get();
}