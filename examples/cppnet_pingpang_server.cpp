#include <functional>
#include <time.h>
#include <stdio.h>
#include <thread>
#include <iostream>
#include <assert.h>
#include <chrono>

#include "systemlib.h"
#include "socketlibfunction.h"

#include "eventloop.h"
#include "datasocket.h"
#include "TCPServer.h"
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

int main()
{
    double total_recv_len = 0;
    double  packet_num = 0;

    int thread_num;

    std::cout << "enter tcp server eventloop thread num:";
    std::cin >> thread_num;

    int port_num;
    std::cout << "enter port:";
    std::cin >> port_num;

    ox_socket_init();
    
    int total_client_num = 0;

    MsgQueue<NetMsg*>  msgList; /*  用于网络IO线程发送网络消息给逻辑线程 */
    EventLoop       mainLoop;

    TcpServer t(port_num, thread_num, [&](EventLoop& l){
        /*每帧回调函数里强制同步rwlist*/
        if (true)
        {
            lockStatistics();
            msgList.ForceSyncWrite();
            unLockStatistics();

            if (msgList.SharedListSize() > 0)
            {
                mainLoop.wakeup();
            }
        }
    });
    t.setEnterHandle([&](int64_t id){
        if (true)
        {
            NetMsg* msg = new NetMsg(NMT_ENTER, id);
            lockStatistics();
            msgList.Push(msg);
            unLockStatistics();

            mainLoop.wakeup();
        }
        else
        {
            lockStatistics();
            total_client_num++;
            unLockStatistics();
        }
    });

    t.setDisconnectHandle([&](int64_t id){
        if (true)
        {
            NetMsg* msg = new NetMsg(NMT_CLOSE, id);
            lockStatistics();
            msgList.Push(msg);
            unLockStatistics();

            mainLoop.wakeup();
        }
        else
        {
            lockStatistics();
            total_client_num--;
            unLockStatistics();
        }
    });

    t.setMsgHandle([&](int64_t id, const char* buffer, int len){
        if (true)
        {
            NetMsg* msg = new NetMsg(NMT_RECV_DATA, id);
            msg->setData(buffer, len);
            lockStatistics();
            msgList.Push(msg);
            unLockStatistics();

            mainLoop.wakeup();
        }
        else
        {
            t.send(id, DataSocket::makePacket(buffer, len));
            lockStatistics();
            total_recv_len += len;
            packet_num++;
            unLockStatistics();
            
        }
    });

    /*  主线程处理服务器端IO*/
    int64_t lasttime = ox_getnowtime();

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
                }
                else if (msg->mType == NMT_CLOSE)
                {
                    printf("client %lld close \n", msg->mID);
                    total_client_num--;
                }
                else if (msg->mType == NMT_RECV_DATA)
                {
                    t.send(msg->mID, DataSocket::makePacket(msg->mData.c_str(), msg->mData.size()));
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
}