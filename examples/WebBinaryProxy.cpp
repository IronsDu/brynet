#include <iostream>
#include <string>
#include <thread>

#include <brynet/net/http/HttpService.hpp>
#include <brynet/net/http/WebSocketFormat.hpp>
#include <brynet/net/AsyncConnector.hpp>
#include <brynet/net/ListenThread.hpp>
#include <brynet/base/AppStatus.hpp>

using namespace std;
using namespace brynet;
using namespace brynet::net;

int main(int argc, char **argv)
{
    if (argc != 4)
    {
        fprintf(stderr, "Usage: <listen port> <backend ip> <backend port>");
        exit(-1);
    }

    int bindPort = atoi(argv[1]);
    string backendIP = argv[2];
    int backendPort = atoi(argv[3]);

    auto tcpService = TcpService::Create();
    tcpService->startWorkerThread(std::thread::hardware_concurrency());

    auto asyncConnector = AsyncConnector::Create();
    asyncConnector->startWorkerThread();

    auto listenThread = ListenThread::Create(false,
        "0.0.0.0",
        bindPort,
        [tcpService, asyncConnector, backendIP, backendPort](TcpSocket::Ptr socket) {
            auto enterCallback = [tcpService, asyncConnector, backendIP, backendPort](const TcpConnection::Ptr& session) {
                session->setUD(static_cast<int64_t>(1));
                std::shared_ptr<TcpConnection::Ptr> shareBackendSession = std::make_shared<TcpConnection::Ptr>(nullptr);
                std::shared_ptr<std::vector<string>> cachePacket = std::make_shared<std::vector<std::string>>();

                auto enterCallback = [tcpService, session, shareBackendSession, cachePacket](TcpSocket::Ptr socket) {
                    auto enterCallback = [=](const TcpConnection::Ptr& backendSession) {
                        auto ud = brynet::base::cast<int64_t>(session->getUD());
                        if (*ud == -1)   /*if http client already close*/
                        {
                            backendSession->postDisConnect();
                            return;
                        }

                        *shareBackendSession = backendSession;

                        for (auto& p : *cachePacket)
                        {
                            backendSession->send(p.c_str(), p.size());
                        }
                        cachePacket->clear();

                        backendSession->setDisConnectCallback([=](const TcpConnection::Ptr& backendSession) {
                            (void)backendSession;
                            *shareBackendSession = nullptr;
                            auto ud = brynet::base::cast<int64_t>(session->getUD());
                            if (*ud != -1)
                            {
                                session->postDisConnect();
                            }
                            });

                        backendSession->setDataCallback([=](const char* buffer,
                            size_t size) {
                                /* recieve data from backend server, then send to http client */
                                session->send(buffer, size);
                                return size;
                            });
                    };

                    tcpService->addTcpConnection(std::move(socket),
                        brynet::net::AddSocketOption::AddEnterCallback(enterCallback),
                        brynet::net::AddSocketOption::WithMaxRecvBufferSize(32 * 1024));
                };
               

                /* new connect to backend server */
                asyncConnector->asyncConnect({
                    ConnectOption::WithAddr(backendIP.c_str(), backendPort),
                    ConnectOption::WithTimeout(std::chrono::seconds(10)),
                    ConnectOption::WithCompletedCallback(enterCallback) });

                session->setDataCallback([=](const char* buffer, size_t size) {
                    TcpConnection::Ptr backendSession = *shareBackendSession;
                    if (backendSession == nullptr)
                    {
                        /*cache it*/
                        cachePacket->push_back(std::string(buffer, size));
                    }
                    else
                    {
                        /* receive data from http client, then send to backend server */
                        backendSession->send(buffer, size);
                    }

                    return size;
                    });

                session->setDisConnectCallback([=](const TcpConnection::Ptr& session) {
                    /*if http client close, then close it's backend server */
                    TcpConnection::Ptr backendSession = *shareBackendSession;
                    if (backendSession != nullptr)
                    {
                        backendSession->postDisConnect();
                    }

                    session->setUD(-1);
                    });
            };

            tcpService->addTcpConnection(std::move(socket),
                brynet::net::AddSocketOption::AddEnterCallback(enterCallback),
                brynet::net::AddSocketOption::WithMaxRecvBufferSize(32 * 1024));
        });

    // listen for front http client
    listenThread->startListen();
    while(true)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (brynet::base::app_kbhit())
        {
            break;
        }
    }

    return 0;
}