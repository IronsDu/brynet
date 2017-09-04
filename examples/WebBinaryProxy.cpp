#include <iostream>
#include <string>

#include <brynet/net/SocketLibFunction.h>
#include <brynet/net/http/HttpService.h>
#include <brynet/net/http/HttpFormat.h>
#include <brynet/net/http/WebSocketFormat.h>
#include <brynet/net/Connector.h>
#include <brynet/utils/systemlib.h>

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
    tcpService->startWorkThread(ox_getcpunum());

    auto asyncConnector = AsyncConnector::Create();
    asyncConnector->startThread();

    auto listenThread = ListenThread::Create();

    // listen for front http client
    listenThread->startListen(false, "0.0.0.0", bindPort, [tcpService, asyncConnector, backendIP, backendPort](sock fd) {
        tcpService->addSession(fd, [tcpService, asyncConnector, backendIP, backendPort](const TCPSession::PTR& session) {
            HttpService::setup(session, [tcpService, asyncConnector, backendIP, backendPort](const HttpSession::PTR& clientWebSession) {
                clientWebSession->setUD(static_cast<int>(1));
                std::shared_ptr<TCPSession::PTR> shareBackendSession = std::make_shared<TCPSession::PTR>(nullptr);
                std::shared_ptr<std::vector<string>> cachePacket = std::make_shared<std::vector<std::string>>();

                /* new connect to backend server */
                asyncConnector->asyncConnect(backendIP.c_str(), backendPort, 10000, [tcpService, clientWebSession, shareBackendSession, cachePacket](sock fd) {
                    tcpService->addSession(fd, [=](const TCPSession::PTR& backendSession) {
                        auto ud = brynet::net::cast<int>(clientWebSession->getUD());
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
                            auto ud = brynet::net::cast<int>(clientWebSession->getUD());
                            if (*ud != -1)
                            {
                                clientWebSession->postClose();
                            }
                        });

                        backendSession->setDataCallback([=](const TCPSession::PTR& backendSession, const char* buffer, size_t size) {
                            /* recieve data from backend server, then send to http client */
                            size_t totalProcLen = 0;

                            const char* parse_str = buffer;
                            PACKET_LEN_TYPE left_len = size;

                            while (true)
                            {
                                bool flag = false;
                                if (left_len >= PACKET_HEAD_LEN)
                                {
                                    PACKET_LEN_TYPE packet_len = (*(PACKET_LEN_TYPE*)parse_str);
                                    if (left_len >= packet_len && packet_len >= PACKET_HEAD_LEN)
                                    {
                                        std::string sendPayload(parse_str, packet_len);
                                        std::string sendFrame;
                                        WebSocketFormat::wsFrameBuild(sendPayload, sendFrame);

                                        clientWebSession->send(sendFrame.c_str(), sendFrame.size());

                                        totalProcLen += packet_len;
                                        parse_str += packet_len;
                                        left_len -= packet_len;
                                        flag = true;
                                    }
                                }

                                if (!flag)
                                {
                                    break;
                                }
                            }

                            return totalProcLen;
                        });
                    }, false, nullptr, 32*1024, true); //TODO
                }, nullptr);

                clientWebSession->setWSCallback([=](const HttpSession::PTR&, WebSocketFormat::WebSocketFrameType opcode, const std::string& payload) {
                    TCPSession::PTR backendSession = *shareBackendSession;
                    if (backendSession == nullptr)
                    {
                        /*cache it*/
                        cachePacket->push_back(payload);
                    }
                    else
                    {
                        /* receive data from http client, then send to backend server */
                        backendSession->send(payload.c_str(), payload.size());
                    }
                });

                clientWebSession->setCloseCallback([=](const HttpSession::PTR& clientWebSession) {
                    /*if http client close, then close it's backend server */
                    TCPSession::PTR backendSession = *shareBackendSession;
                    if (backendSession != nullptr)
                    {
                        backendSession->postClose();
                    }

                    clientWebSession->setUD(-1);
                });
            });
        }, false, nullptr, 1024*1024);
    });
    
    std::cin.get();
}