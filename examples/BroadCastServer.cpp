#include <functional>
#include <time.h>
#include <stdio.h>
#include <thread>
#include <iostream>
#include <assert.h>
#include <chrono>
#include <vector>
#include <atomic>
#include <shared_mutex>

#include "packet.h"
#include "systemlib.h"
#include "SocketLibFunction.h"

#include "EventLoop.h"
#include "DataSocket.h"
#include "TCPService.h"
#include "MsgQueue.h"

using namespace brynet;
using namespace brynet::net;

std::atomic_llong TotalSendLen = ATOMIC_VAR_INIT(0);
std::atomic_llong TotalRecvLen = ATOMIC_VAR_INIT(0);

std::atomic_llong  SendPacketNum = ATOMIC_VAR_INIT(0);
std::atomic_llong  RecvPacketNum = ATOMIC_VAR_INIT(0);

std::vector<TcpService::SESSION_TYPE> clients;
std::shared_mutex clientGurad;
TcpService::PTR service;

static void addClientID(TcpService::SESSION_TYPE id)
{
	std::unique_lock<std::shared_mutex> lock(clientGurad);
	clients.push_back(id);
}

static void removeClientID(TcpService::SESSION_TYPE id)
{
	std::unique_lock<std::shared_mutex> lock(clientGurad);
	clients.erase(std::find(clients.begin(), clients.end(), id));
}

static size_t getClientNum()
{
	std::shared_lock<std::shared_mutex> lock(clientGurad);
	return clients.size();
}

static void broadCastPacket(const TcpService::PTR& service, DataSocket::PACKET_PTR packet)
{
	std::shared_lock<std::shared_mutex> lock(clientGurad);
	auto packetLen = packet->size();
	RecvPacketNum++;
	TotalRecvLen += packetLen;
	std::for_each(clients.begin(), clients.end(), [&](TcpService::SESSION_TYPE id) {
		service->send(id, packet);
		SendPacketNum++;
		TotalSendLen += packetLen;
	});
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

	service = TcpService::Create();
    auto mainLoop = std::make_shared<EventLoop>();
    
    service->startListen(false, "0.0.0.0", port, 1024 * 1024, nullptr, nullptr);
    service->startWorkerThread(2, [](EventLoop::PTR l){
    });

    service->setEnterCallback([](int64_t id, std::string ip){
		addClientID(id);
    });

    service->setDisconnectCallback([](int64_t id){
		removeClientID(id);
    });

    service->setDataCallback([mainLoop](int64_t id, const char* buffer, size_t len){
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
						broadCastPacket(service, std::move(packet));
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

	int64_t now = ox_getnowtime();
    while (true)
    {
        mainLoop->loop(1000);
		if ((ox_getnowtime() - now) >= 1000)
		{
			std::cout << "clientnum:" << getClientNum() << ", recv" << (TotalRecvLen / 1024) << " K/s, " << "num : " << RecvPacketNum << ", send " <<
				(TotalSendLen / 1024) / 1024 << " M/s, " << " num: " << SendPacketNum << std::endl;
			TotalRecvLen = 0;
			TotalSendLen = 0;
			RecvPacketNum = 0;
			SendPacketNum = 0;
			now = ox_getnowtime();
		}
    }

    service->closeService();

	return 0;
}
