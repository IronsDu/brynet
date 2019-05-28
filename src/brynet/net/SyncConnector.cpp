#include <future>
#include <memory>
#include <chrono>

#include <brynet/net/SyncConnector.h>

namespace brynet { namespace net {

    TcpSocket::Ptr SyncConnectSocket(std::vector<AsyncConnector::ConnectOptions::ConnectOptionFunc> connectOptions,
        AsyncConnector::Ptr asyncConnector)
    {
        if (asyncConnector == nullptr)
        {
            asyncConnector = AsyncConnector::Create();
            asyncConnector->startWorkerThread();
        }
        auto timeout = AsyncConnector::ConnectOptions::ExtractTimeout(connectOptions);

        auto socketPromise = std::make_shared<std::promise<TcpSocket::Ptr>>();
        connectOptions.push_back(AsyncConnector::ConnectOptions::WithCompletedCallback([socketPromise](TcpSocket::Ptr socket) {
            socketPromise->set_value(std::move(socket));
        }));
        connectOptions.push_back(AsyncConnector::ConnectOptions::WithFailedCallback([socketPromise]() {
            socketPromise->set_value(nullptr);
        }));

        asyncConnector->asyncConnect(connectOptions);

        auto future = socketPromise->get_future();
        if (future.wait_for(timeout) != std::future_status::ready)
        {
            return nullptr;
        }

        return future.get();
    }

    TcpConnection::Ptr SyncConnectSession(TcpService::Ptr service,
        std::vector<AsyncConnector::ConnectOptions::ConnectOptionFunc> connectOptions,
        std::vector<TcpService::AddSocketOption::AddSocketOptionFunc> socketOptions,
        AsyncConnector::Ptr asyncConnector)
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
        auto timeout = AsyncConnector::ConnectOptions::ExtractTimeout(connectOptions);

        auto sessionPromise = std::make_shared<std::promise<TcpConnection::Ptr>>();
        connectOptions.push_back(AsyncConnector::ConnectOptions::WithCompletedCallback([=](TcpSocket::Ptr socket) mutable {
            socket->setNodelay();

            auto enterCallback = [sessionPromise](const TcpConnection::Ptr& session) mutable {
                sessionPromise->set_value(session);
            };
            socketOptions.push_back(TcpService::AddSocketOption::AddEnterCallback(enterCallback));
            service->addTcpConnection(std::move(socket), socketOptions);
        }));
        connectOptions.push_back(AsyncConnector::ConnectOptions::WithFailedCallback([sessionPromise]() {
            sessionPromise->set_value(nullptr);
        }));

        asyncConnector->asyncConnect(connectOptions);

        auto future = sessionPromise->get_future();
        if (future.wait_for(timeout) != std::future_status::ready)
        {
            return nullptr;
        }

        return future.get();
    }

} }
