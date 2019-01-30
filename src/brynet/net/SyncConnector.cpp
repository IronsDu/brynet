#include <future>
#include <memory>
#include <chrono>

#include <brynet/net/SyncConnector.h>

namespace brynet { namespace net {

    TcpSocket::Ptr SyncConnectSocket(std::string ip,
        int port,
        std::chrono::milliseconds timeout,
        AsyncConnector::Ptr asyncConnector)
    {
        if (asyncConnector == nullptr)
        {
            asyncConnector = AsyncConnector::Create();
            asyncConnector->startWorkerThread();
        }

        auto socketPromise = std::make_shared<std::promise<TcpSocket::Ptr>>();
        asyncConnector->asyncConnect(
            std::move(ip),
            port,
            timeout,
            [socketPromise](TcpSocket::Ptr socket) {
            socketPromise->set_value(std::move(socket));
        }, [socketPromise]() {
            socketPromise->set_value(nullptr);
        });

        return socketPromise->get_future().get();
    }

    TcpConnection::Ptr SyncConnectSession(std::string ip,
        int port,
        std::chrono::milliseconds timeout,
        TcpService::Ptr service,
        const std::vector<TcpService::AddSocketOption::AddSocketOptionFunc>& options,
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

        auto sessionPromise = std::make_shared<std::promise<TcpConnection::Ptr>>();
        asyncConnector->asyncConnect(
            std::move(ip),
            port,
            timeout,
            [=](TcpSocket::Ptr socket) mutable {
            socket->setNodelay();

            auto enterCallback = [sessionPromise](const TcpConnection::Ptr& session) mutable {
                sessionPromise->set_value(session);
            };
            std::vector < TcpService::AddSocketOption::AddSocketOptionFunc> tmpOptions = options;
            tmpOptions.push_back(TcpService::AddSocketOption::WithEnterCallback(enterCallback));
            service->addTcpConnection(std::move(socket), tmpOptions);
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

} }
