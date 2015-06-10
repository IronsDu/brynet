#include <functional>
#include <time.h>
#include <stdio.h>
#include <thread>
#include <iostream>
#include <assert.h>
#include <chrono>
#include <vector>

#include "packet.h"
#include "systemlib.h"
#include "socketlibfunction.h"

#include "eventloop.h"
#include "datasocket.h"
#include "TCPService.h"
#include "msgqueue.h"

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

    void        setData(const char* data, int len)
    {
        mData = std::string(data, len);
    }

    NetMsgType  mType;
    int64_t     mID;
    std::string      mData;
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

int main(int argc, char **argv)
{
    double total_recv_len = 0;
    double  packet_num = 0;
    int total_client_num = 0;

    int thread_num = atoi(argv[1]);
    int port_num = atoi(argv[2]);

    ox_socket_init();

    MsgQueue<NetMsg*>  msgList;
    EventLoop       mainLoop;

    TcpService t;
    t.startListen(port_num, nullptr, nullptr);
    t.startWorkerThread(thread_num, [&](EventLoop& l){
        lockStatistics();
        msgList.ForceSyncWrite();
        unLockStatistics();

        if (msgList.SharedListSize() > 0)
        {
            mainLoop.wakeup();
        }
    });

    t.setEnterCallback([&](int64_t id, std::string ip){
        NetMsg* msg = new NetMsg(NMT_ENTER, id);
        lockStatistics();
        msgList.Push(msg);
        unLockStatistics();

        mainLoop.wakeup();
    });

    t.setDisconnectCallback([&](int64_t id){
        NetMsg* msg = new NetMsg(NMT_CLOSE, id);
        lockStatistics();
        msgList.Push(msg);
        unLockStatistics();

        mainLoop.wakeup();
    });

    t.setDataCallback([&](int64_t id, const char* buffer, int len){
        NetMsg* msg = new NetMsg(NMT_RECV_DATA, id);
        msg->setData(buffer, len);
        lockStatistics();
        msgList.Push(msg);
        unLockStatistics();

        return len;
    });

    int64_t lasttime = ox_getnowtime();
    int total_count = 0;

    std::vector<int64_t> sessions;
    mainLoop.restoreThreadID();
    while (true)
    {
        mainLoop.loop(10);

        msgList.SyncRead(0);
        NetMsg* msg = nullptr;
        while (msgList.ReadListSize() > 0)
        {
            bool ret = msgList.PopFront(&msg);
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
                    total_client_num--;
                }
                else if (msg->mType == NMT_RECV_DATA)
                {
                    DataSocket::PACKET_PTR packet = DataSocket::makePacket(msg->mData.c_str(), msg->mData.size());
                    t.send(msg->mID, packet);
                    total_recv_len += msg->mData.size();
                    packet_num++;
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
            std::cout << "recv by clientnum:" << total_client_num << " of :" << (total_recv_len / 1024) / 1024 << " M / s, " << "packet num : " << packet_num << std::endl;
            lasttime = now;
            total_recv_len = 0;
            packet_num = 0;
        }
    }

    t.closeService();
}