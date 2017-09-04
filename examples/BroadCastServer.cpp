#include <functional>
#include <time.h>
#include <stdio.h>
#include <thread>
#include <iostream>
#include <assert.h>
#include <chrono>
#include <vector>
#include <atomic>

#include <brynet/utils/packet.h>
#include <brynet/utils/systemlib.h>
#include <brynet/net/SocketLibFunction.h>

#include <brynet/net/EventLoop.h>
#include <brynet/net/DataSocket.h>
#include <brynet/net/WrapTCPService.h>
#include <brynet/net/ListenThread.h>

using namespace brynet;
using namespace brynet::net;

std::atomic_llong TotalSendLen = ATOMIC_VAR_INIT(0);
std::atomic_llong TotalRecvLen = ATOMIC_VAR_INIT(0);

std::atomic_llong  SendPacketNum = ATOMIC_VAR_INIT(0);
std::atomic_llong  RecvPacketNum = ATOMIC_VAR_INIT(0);

std::vector<TCPSession::PTR> clients;
WrapTcpService::PTR service;

static void addClientID(const TCPSession::PTR& session)
{
    clients.push_back(session);
}

static void removeClientID(const TCPSession::PTR& session)
{
    for (auto it = clients.begin(); it != clients.end(); ++it)
    {
        if ((*it)->getSocketID() == session->getSocketID())
        {
            clients.erase(it);
            break;
        }
    }
}

static size_t getClientNum()
{
    return clients.size();
}

static void broadCastPacket(const DataSocket::PACKET_PTR& packet)
{
    auto packetLen = packet->size();
    RecvPacketNum++;
    TotalRecvLen += packetLen;

    for (const auto& session : clients)
    {
        session->send(packet);
    }

    SendPacketNum += clients.size();
    TotalSendLen += (clients.size() * packetLen);
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage : <listen port> \n");
        exit(-1);
    }

    int port = atoi(argv[1]);
    ox_socket_init();

    service = std::make_shared<WrapTcpService>();
    auto mainLoop = std::make_shared<EventLoop>();
    auto listenThrean = ListenThread::Create();

    listenThrean->startListen(false, "0.0.0.0", port, [mainLoop, listenThrean](sock fd) {
        ox_socket_nodelay(fd);
        ox_socket_setsdsize(fd, 32 * 1024);
        ox_socket_setrdsize(fd, 32 * 1024);
        service->addSession(fd, [mainLoop](const TCPSession::PTR& session) {
            mainLoop->pushAsyncProc([session]() {
                addClientID(session);
            });

            session->setCloseCallback([mainLoop](const TCPSession::PTR& session) {
                mainLoop->pushAsyncProc([session]() {
                    removeClientID(session);
                });
            });

            session->setDataCallback([mainLoop](const TCPSession::PTR& session, const char* buffer, size_t len) {
                const char* parseStr = buffer;
                size_t totalProcLen = 0;
                size_t leftLen = len;

                while (true)
                {
                    bool flag = false;
                    if (leftLen >= PACKET_HEAD_LEN)
                    {
                        ReadPacket rp(parseStr, leftLen);
                        PACKET_LEN_TYPE packet_len = rp.readPacketLen();
                        if (leftLen >= packet_len && packet_len >= PACKET_HEAD_LEN)
                        {
                            mainLoop->pushAsyncProc([packet = DataSocket::makePacket(parseStr, packet_len)]() {
                                broadCastPacket(packet);
                            });

                            totalProcLen += packet_len;
                            parseStr += packet_len;
                            leftLen -= packet_len;
                            flag = true;
                        }
                        rp.skipAll();
                    }

                    if (!flag)
                    {
                        break;
                    }
                }

                return totalProcLen;
            });
        }, false, listenThrean, 1024 * 1024, false);
    });

    service->startWorkThread(2);

    auto now = std::chrono::steady_clock::now();
    while (true)
    {
        mainLoop->loop(10);
        auto diff = std::chrono::steady_clock::now() - now;
        if (diff >= std::chrono::seconds(1))
        {
            auto msDiff = std::chrono::duration_cast<std::chrono::milliseconds>(diff).count();
            std::cout << "cost " << msDiff << " ms, clientnum:" << getClientNum() << ", recv" << (TotalRecvLen / 1024) * 1000 / msDiff  << " K/s, " << "num : " << RecvPacketNum * 1000 / msDiff << ", send " <<
                (TotalSendLen / 1024) / 1024 * 1000 / msDiff << " M/s, " << " num: " << SendPacketNum * 1000 / msDiff << std::endl;
            TotalRecvLen = 0;
            TotalSendLen = 0;
            RecvPacketNum = 0;
            SendPacketNum = 0;
            now = std::chrono::steady_clock::now();
        }
    }

    service->stopWorkThread();

    return 0;
}
