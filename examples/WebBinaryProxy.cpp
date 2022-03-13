﻿#include <brynet/base/AppStatus.hpp>
#include <brynet/net/AsyncConnector.hpp>
#include <brynet/net/ListenThread.hpp>
#include <brynet/net/http/HttpService.hpp>
#include <brynet/net/http/WebSocketFormat.hpp>
#include <iostream>
#include <string>
#include <thread>

using namespace std;
using namespace brynet;
using namespace brynet::net;

int main(int argc, char** argv)
{
    if (argc != 4)
    {
        fprintf(stderr, "Usage: <listen port> <backend ip> <backend port>");
        exit(-1);
    }

    int bindPort = atoi(argv[1]);
    string backendIP = argv[2];
    int backendPort = atoi(argv[3]);

    auto tcpService = IOThreadTcpService::Create();
    tcpService->startWorkerThread(std::thread::hardware_concurrency());

    auto asyncConnector = AsyncConnector::Create();
    asyncConnector->startWorkerThread();

    auto listenThread = ListenThread::Create(false,
                                             "0.0.0.0",
                                             bindPort,
                                             [tcpService, asyncConnector, backendIP, backendPort](TcpSocket::Ptr socket) {
                                                 auto status = std::make_shared<bool>(true);
                                                 auto enterCallback = [tcpService, asyncConnector, backendIP, backendPort, status](const TcpConnection::Ptr& session) {
                                                     std::shared_ptr<TcpConnection::Ptr> shareBackendSession = std::make_shared<TcpConnection::Ptr>(nullptr);
                                                     std::shared_ptr<std::vector<string>> cachePacket = std::make_shared<std::vector<std::string>>();

                                                     auto enterCallback = [tcpService, session, shareBackendSession, cachePacket, status](TcpSocket::Ptr socket) {
                                                         auto enterCallback = [=](const TcpConnection::Ptr& backendSession) {
                                                             if (!*status) /*if http client already close*/
                                                             {
                                                                 backendSession->postDisConnect();
                                                                 return;
                                                             }

                                                             *shareBackendSession = backendSession;

                                                             for (auto& p : *cachePacket)
                                                             {
                                                                 backendSession->send(p);
                                                             }
                                                             cachePacket->clear();

                                                             backendSession->setDisConnectCallback([=](const TcpConnection::Ptr& backendSession) {
                                                                 (void) backendSession;
                                                                 *shareBackendSession = nullptr;
                                                                 if (*status)
                                                                 {
                                                                     session->postDisConnect();
                                                                 }
                                                             });

                                                             backendSession->setDataCallback([=](brynet::base::BasePacketReader& reader) {
                                                                 /* receive data from backend server, then send to http client */
                                                                 session->send(reader.begin(), reader.size());
                                                                 reader.consumeAll();
                                                             });
                                                         };

                                                         ConnectionOption option;
                                                         option.enterCallback.emplace_back(enterCallback);
                                                         option.maxRecvBufferSize = 32 * 1024;
                                                         tcpService->addTcpConnection(std::move(socket), option);
                                                     };

                                                     /* new connect to backend server */
                                                     ConnectOption option;
                                                     option.ip = backendIP;
                                                     option.port = backendPort;
                                                     option.timeout = std::chrono::seconds(10);
                                                     option.completedCallback = enterCallback;
                                                     asyncConnector->asyncConnect(option);

                                                     session->setDataCallback([=](brynet::base::BasePacketReader& reader) {
                                                         TcpConnection::Ptr backendSession = *shareBackendSession;
                                                         if (backendSession == nullptr)
                                                         {
                                                             /*cache it*/
                                                             cachePacket->push_back(std::string(reader.begin(), reader.size()));
                                                         }
                                                         else
                                                         {
                                                             /* receive data from http client, then send to backend server */
                                                             backendSession->send(reader.begin(), reader.size());
                                                         }

                                                         reader.consumeAll();
                                                     });

                                                     session->setDisConnectCallback([=](const TcpConnection::Ptr& session) {
                                                         /*if http client close, then close it's backend server */
                                                         TcpConnection::Ptr backendSession = *shareBackendSession;
                                                         if (backendSession != nullptr)
                                                         {
                                                             backendSession->postDisConnect();
                                                         }
                                                         *status = false;
                                                     });
                                                 };

                                                 ConnectionOption option;
                                                 option.enterCallback.emplace_back(enterCallback);
                                                 option.maxRecvBufferSize = 32 * 1024;
                                                 tcpService->addTcpConnection(std::move(socket), option);
                                             });

    // listen for front http client
    listenThread->startListen();
    while (true)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (brynet::base::app_kbhit())
        {
            break;
        }
    }

    return 0;
}
