#include <functional>
#include <time.h>
#include <stdio.h>
#include <thread>
#include <iostream>
#include <assert.h>
#include <chrono>
#include <vector>
#include <atomic>

#include "packet.h"
#include "systemlib.h"
#include "SocketLibFunction.h"

#include "EventLoop.h"
#include "DataSocket.h"
#include "TCPService.h"
#include "MsgQueue.h"

using namespace dodo;
using namespace dodo::net;

enum NetMsgType
{
    NMT_ENTER,      /*链接进入*/
    NMT_CLOSE,      /*链接断开*/
    NMT_RECV_DATA,  /*收到消息*/
};

struct NetMsg
{
    NetMsg(NetMsgType t, int64_t id) : mType(t), mID(id)
    {
    }

    void        setData(const char* data, size_t len)
    {
        mData = std::string(data, len);
    }

    NetMsgType  mType;
    int64_t     mID;
    std::string mData;
};

std::mutex                      gTatisticsCppMutex;

void    lockStatistics()
{
    gTatisticsCppMutex.lock();
}

void    unLockStatistics()
{
    gTatisticsCppMutex.unlock();
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage : <listen port> \n");
        exit(-1);
    }

    int port_num = atoi(argv[1]);

    std::atomic_llong total_send_len = ATOMIC_VAR_INIT(0);
    std::atomic_llong total_recv_len = ATOMIC_VAR_INIT(0);

    std::atomic_llong  send_packet_num = ATOMIC_VAR_INIT(0);
    std::atomic_llong  recv_packet_num = ATOMIC_VAR_INIT(0);

    ox_socket_init();

    int total_client_num = 0;

    /*  用于网络IO线程发送消息给逻辑线程的消息队列,当网络线程的回调函数push消息后，需要wakeup主线程 */
    /*  当然，TcpServer的各个回调函数中可以自己处理消息，而不必发送到msgList队列    */
    MsgQueue<NetMsg*>  msgList;
    EventLoop       mainLoop;

    TcpService t;
    t.startListen(false, "0.0.0.0", port_num, 1024 * 1024, nullptr, nullptr);
    t.startWorkerThread(2, [&](EventLoop::PTR l){
        /*每帧回调函数里强制同步rwlist*/
        lockStatistics();
        msgList.forceSyncWrite();
        unLockStatistics();

        if (msgList.sharedListSize() > 0)
        {
            mainLoop.wakeup();
        }
    });

    t.setEnterCallback([&](int64_t id, std::string ip){
        NetMsg* msg = new NetMsg(NMT_ENTER, id);
        lockStatistics();
        msgList.push(msg);
        unLockStatistics();

        mainLoop.wakeup();
    });

    t.setDisconnectCallback([&](int64_t id){
        NetMsg* msg = new NetMsg(NMT_CLOSE, id);
        lockStatistics();
        msgList.push(msg);
        unLockStatistics();

        mainLoop.wakeup();
    });

    t.setDataCallback([&](int64_t id, const char* buffer, size_t len){
        const char* parse_str = buffer;
        size_t total_proc_len = 0;
        size_t left_len = len;

        while (true)
        {
            bool flag = false;
            if (left_len >= PACKET_HEAD_LEN)
            {
                ReadPacket rp(parse_str, left_len);
                PACKET_LEN_TYPE packet_len = rp.readPacketLen();
                if (left_len >= packet_len && packet_len >= PACKET_HEAD_LEN)
                {
                    NetMsg* msg = new NetMsg(NMT_RECV_DATA, id);
                    msg->setData(parse_str, packet_len);
                    lockStatistics();
                    msgList.push(msg);
                    unLockStatistics();

                    total_proc_len += packet_len;
                    parse_str += packet_len;
                    left_len -= packet_len;
                    flag = true;
                }
                rp.skipAll();
            }

            if (!flag)
            {
                break;
            }
        }

        return total_proc_len;
    });

    /*  主线程处理msgList消息队列    */
    int64_t lasttime = ox_getnowtime();
    int total_count = 0;

    std::vector<int64_t> sessions;

    while (true)
    {
        mainLoop.loop(10);

        msgList.syncRead(0);
        NetMsg* msg = nullptr;
        while (msgList.readListSize() > 0)
        {
            bool ret = msgList.popFront(msg);
            if (ret)
            {
                if (msg->mType == NMT_ENTER)
                {
                    printf("client %lld enter \n", msg->mID);
                    total_client_num++;
                    sessions.push_back(msg->mID);
                }
                else if (msg->mType == NMT_CLOSE)
                {
                    printf("client %lld close \n", msg->mID);
                    for (auto it = sessions.begin(); it != sessions.end(); ++it)
                    {
                        if (*it == msg->mID)
                        {
                            sessions.erase(it);
                            break;
                        }
                    }
                    total_client_num--;
                }
                else if (msg->mType == NMT_RECV_DATA)
                {
                    DataSocket::PACKET_PTR packet = DataSocket::makePacket(msg->mData.c_str(), msg->mData.size());
                    recv_packet_num++;
                    total_recv_len += msg->mData.size();

                    for (size_t i = 0; i < sessions.size(); ++i)
                    {
                        t.send(sessions[i], packet);
                        send_packet_num++;
                        total_send_len += msg->mData.size();
                    }
                }
                else
                {
                    assert(false);
                }

                delete msg;
                msg = nullptr;
            }
            else
            {
                break;
            }
        }
        int64_t now = ox_getnowtime();
        if ((now - lasttime) >= 1000)
        {
            std::cout << "clientnum:" << total_client_num << ", recv" << (total_recv_len / 1024) << " K/s, " << "num : " << recv_packet_num << ", send " << 
                (total_send_len / 1024) / 1024 << " M/s, " << " num: " << send_packet_num << std::endl;
            lasttime = now;
            total_recv_len = 0;
            total_send_len = 0;
            recv_packet_num = 0;
            send_packet_num = 0;
        }
    }

    t.closeService();
}
