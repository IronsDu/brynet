#include <stdio.h>
#include <iostream>
#include <chrono>
#include <memory>
#include <atomic>

#include <brynet/base/Packet.hpp>

#include <brynet/net/SocketLibFunction.hpp>
#include <brynet/net/EventLoop.hpp>
#include <brynet/net/TcpConnection.hpp>
#include <brynet/base/AppStatus.hpp>

using namespace std;
using namespace brynet;
using namespace brynet::net;
using namespace brynet::base;

atomic_llong  TotalRecvPacketNum = ATOMIC_VAR_INIT(0);
atomic_llong TotalRecvSize = ATOMIC_VAR_INIT(0);

int main(int argc, char** argv)
{
    if (argc != 5)
    {
        fprintf(stderr, "Usage: <server ip> <server port> <session num> <packet size>\n");
        exit(-1);
    }

    std::string ip = argv[1];
    int port = atoi(argv[2]);
    int clientNum = atoi(argv[3]);
    int packetLen = atoi(argv[4]);

    brynet::net::base::InitSocket();

    auto clientEventLoop = std::make_shared<EventLoop>();

    for (int i = 0; i < clientNum; i++)
    {
        auto fd = brynet::net::base::Connect(false, ip, port);
        brynet::net::base::SocketSetSendSize(fd, 32 * 1024);
        brynet::net::base::SocketSetRecvSize(fd, 32 * 1024);
        brynet::net::base::SocketNodelay(fd);

        auto enterCallback = [packetLen](const TcpConnection::Ptr& dataSocket) {
            static_assert(sizeof(dataSocket.get()) <= sizeof(int64_t), "ud's size must less int64");

            auto HEAD_LEN = sizeof(uint32_t) + sizeof(uint16_t);

            std::shared_ptr<BigPacket> sp = std::make_shared<BigPacket>(false);
            sp->writeUINT32(HEAD_LEN + sizeof(int64_t) + packetLen);
            sp->writeUINT16(1);
            sp->writeINT64((int64_t)dataSocket.get());
            sp->writeBinary(std::string(packetLen, '_'));

            for (int i = 0; i < 1; ++i)
            {
                dataSocket->send(sp->getData(), sp->getPos());
            }

            dataSocket->setDataCallback([dataSocket](brynet::base::BasePacketReader& reader) {
                while (true)
                {
                    if (!reader.enough(sizeof(uint32_t)))
                    {
                        break;
                    }

                    auto buffer = reader.currentBuffer();
                    auto packetLen = reader.readUINT32();
                    if (!reader.enough(packetLen-sizeof(uint32_t)))
                    {
                        break;
                    }

                    TotalRecvSize += packetLen;
                    TotalRecvPacketNum++;

                    reader.readUINT16();
                    int64_t addr = reader.readINT64();

                    if (addr == (int64_t)(dataSocket.get()))
                    {
                        dataSocket->send(buffer, packetLen);
                    }

                    reader.addPos(packetLen-sizeof(uint32_t)-sizeof(uint16_t)-sizeof(int64_t));
                    reader.savePos();
                }
            });

            dataSocket->setDisConnectCallback([](const TcpConnection::Ptr& dataSocket) {
                (void)dataSocket;
            });
        };
        auto tcpConnection = TcpConnection::Create(TcpSocket::Create(fd, false),
            1024 * 1024, 
            enterCallback, 
            clientEventLoop);
    }

    auto now = std::chrono::steady_clock::now();
    while (true)
    {
        clientEventLoop->loop(10);
        if ((std::chrono::steady_clock::now() - now) >= std::chrono::seconds(1))
        {
            if (TotalRecvSize / 1024 == 0)
            {
                std::cout << "total recv : " << TotalRecvSize << " bytes/s" << ", num " << TotalRecvPacketNum << endl;
            }
            else if ((TotalRecvSize / 1024) / 1024 == 0)
            {
                std::cout << "total recv : " << TotalRecvSize / 1024 << " K/s" << ", num " << TotalRecvPacketNum << endl;
            }
            else
            {
                std::cout << "total recv : " << (TotalRecvSize / 1024) / 1024 << " M/s" << ", num " << TotalRecvPacketNum << endl;
            }

            now = std::chrono::steady_clock::now();
            TotalRecvSize = 0;
            TotalRecvPacketNum = 0;
        }
        if (app_kbhit())
        {
            break;
        }
    }

    return 0;
}
