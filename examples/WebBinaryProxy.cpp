#include <iostream>
#include <string>

#include "SocketLibFunction.h"
#include "HttpServer.h"
#include "HttpFormat.h"
#include "WebSocketFormat.h"

typedef  uint32_t PACKET_LEN_TYPE;

const static PACKET_LEN_TYPE PACKET_HEAD_LEN = sizeof(PACKET_LEN_TYPE);

using namespace std;
using namespace dodo;
using namespace dodo::net;

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

    HttpServer::PTR netService = std::make_shared<HttpServer>();

    netService->startListen(false, "0.0.0.0", bindPort);
    netService->startWorkThread(ox_getcpunum());

    netService->setEnterCallback([=](HttpSession::PTR clientWebSession){
        clientWebSession->setUD(1);
        std::shared_ptr<TCPSession::PTR> shareBackendSession = std::make_shared<TCPSession::PTR>(nullptr);
        std::shared_ptr<std::vector<string>> cachePacket = std::make_shared<std::vector<std::string>>();

        /*链接后端服务器*/
        sock fd = ox_socket_connect(false, backendIP.c_str(), backendPort);
        if (fd != SOCKET_ERROR)
        {
            netService->getServer()->addSession(fd, [=](TCPSession::PTR backendSession){
                if (clientWebSession->getUD() == -1)   /*如果客户端提前断开，则也关闭此链接*/
                {
                    backendSession->postClose();
                    return;
                }

                *shareBackendSession = backendSession;

                /*发送缓存的消息包*/
                for (auto& p : *cachePacket)
                {
                    backendSession->send(p.c_str(), p.size());
                }
                cachePacket->clear();

                backendSession->setCloseCallback([=](TCPSession::PTR backendSession){
                    *shareBackendSession = nullptr;
                    if (clientWebSession->getUD() != -1)
                    {
                        clientWebSession->postClose();
                    }
                });

                backendSession->setDataCallback([=](TCPSession::PTR backendSession, const char* buffer, size_t size){
                    /*收到后端服务器的数据*/
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
            }, false, 32 * 1014, true);
        }

        clientWebSession->setWSCallback([=](HttpSession::PTR, WebSocketFormat::WebSocketFrameType opcode, const std::string& payload){
            TCPSession::PTR backendSession = *shareBackendSession;
            if (backendSession == nullptr)
            {
                /*暂存起来*/
                cachePacket->push_back(payload);
            }
            else
            {
                /*转发给对应的后端服务器链接*/
                backendSession->send(payload.c_str(), payload.size());
            }
        });

        clientWebSession->setCloseCallback([=](HttpSession::PTR clientWebSession){
            /*客户端断开连接*/
            TCPSession::PTR backendSession = *shareBackendSession;
            if (backendSession != nullptr)
            {
                backendSession->postClose();
            }

            clientWebSession->setUD(-1);
        });
    });
    
    std::cin.get();
}