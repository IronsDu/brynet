#include <iostream>
#include <string>
#include <thread>

#include <brynet/net/SocketLibFunction.h>
#include <brynet/net/http/HttpService.h>
#include <brynet/net/http/HttpFormat.h>
#include <brynet/net/http/WebSocketFormat.h>
#include <brynet/net/Connector.h>

typedef  uint32_t PACKET_LEN_TYPE;

const static PACKET_LEN_TYPE PACKET_HEAD_LEN = sizeof(PACKET_LEN_TYPE);

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

    auto tcpService = std::make_shared<WrapTcpService>();
    tcpService->startWorkThread(std::thread::hardware_concurrency());

    auto asyncConnector = AsyncConnector::Create();
    asyncConnector->startThread();

    auto listenThread = ListenThread::Create();

    // listen for front http client
    listenThread->startListen(false, "0.0.0.0", bindPort, [tcpService, asyncConnector, backendIP, backendPort](sock fd) {
        tcpService->addSession(fd, [tcpService, asyncConnector, backendIP, backendPort](const TCPSession::PTR& session) {
            session->setUD(static_cast<int64_t>(1));
            std::shared_ptr<TCPSession::PTR> shareBackendSession = std::make_shared<TCPSession::PTR>(nullptr);
            std::shared_ptr<std::vector<string>> cachePacket = std::make_shared<std::vector<std::string>>();

            /* new connect to backend server */
            asyncConnector->asyncConnect(backendIP.c_str(), backendPort, std::chrono::seconds(10), [tcpService, session, shareBackendSession, cachePacket](sock fd) {
                if (true)
                {
                    tcpService->addSession(fd, [=](const TCPSession::PTR& backendSession) {
                        auto ud = brynet::net::cast<int64_t>(session->getUD());
                        if (*ud == -1)   /*if http client already close*/
                        {
                            backendSession->postClose();
                            return;
                        }

                        *shareBackendSession = backendSession;

                        for (auto& p : *cachePacket)
                        {
                            backendSession->send(p.c_str(), p.size());
                        }
                        cachePacket->clear();

                        backendSession->setCloseCallback([=](const TCPSession::PTR& backendSession) {
                            *shareBackendSession = nullptr;
                            auto ud = brynet::net::cast<int64_t>(session->getUD());
                            if (*ud != -1)
                            {
                                session->postClose();
                            }
                        });

                        backendSession->setDataCallback([=](const TCPSession::PTR& backendSession, const char* buffer, size_t size) {
                            /* recieve data from backend server, then send to http client */
                            session->send(buffer, size);
                            return size;
                        });
                    }, false, nullptr, 32 * 1024, false); //TODO
                }
            }, nullptr);

            session->setDataCallback([=](const TCPSession::PTR&, const char* buffer, size_t size) {
                TCPSession::PTR backendSession = *shareBackendSession;
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

            session->setCloseCallback([=](const TCPSession::PTR& session) {
                /*if http client close, then close it's backend server */
                TCPSession::PTR backendSession = *shareBackendSession;
                if (backendSession != nullptr)
                {
                    backendSession->postClose();
                }

                session->setUD(-1);
            });
        }, false, nullptr, 32 * 1024);
    });
    
    std::cin.get();
}