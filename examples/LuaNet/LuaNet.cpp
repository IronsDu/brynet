#include <functional>
#include <iostream>
#include <vector>
#include <string>

#include "systemlib.h"
#include "SocketLibFunction.h"

#include "EventLoop.h"
#include "DataSocket.h"
#include "TCPService.h"
#include "msgqueue.h"
#include "connector.h"

#include "./liblua/lua_tinker.h"

class IdCreator
{
public:
    IdCreator()
    {
        mIncID = 0;
    }

    int64_t claim()
    {
        int64_t id = 0;
        id |= (ox_getnowtime() / 1000 << 32);
        id |= (mIncID++);

        return id;
    }

private:
    int     mIncID;
};

struct AsyncConnectResult
{
    sock fd;
    int64_t uid;
};

enum NetMsgType
{
    NMT_ENTER,      /*链接进入*/
    NMT_CLOSE,      /*链接断开*/
    NMT_RECV_DATA,  /*收到消息*/
    NMT_CONNECTED,  /*向外建立的链接*/
};

struct NetMsg
{
    NetMsg(int serviceID, NetMsgType t, int64_t id) : mServiceID(serviceID), mType(t), mID(id)
    {
    }

    void        setData(const char* data, size_t len)
    {
        mData = std::string(data, len);
    }

    int         mServiceID;
    NetMsgType  mType;
    int64_t     mID;
    std::string mData;
};

struct lua_State* L = nullptr;

struct LuaTcpSession
{
    typedef std::shared_ptr<LuaTcpSession> PTR;

    int64_t mID;
    std::string recvData;
};

struct LuaTcpService
{
    typedef std::shared_ptr<LuaTcpService> PTR;

    LuaTcpService()
    {
        mTcpService = std::make_shared<TcpService>();
    }

    int                                             mServiceID;
    TcpService::PTR                                 mTcpService;
    std::unordered_map<int64_t, LuaTcpSession::PTR> mSessions;
};

static int64_t monitorTime = ox_getnowtime();
static void luaRuntimeCheck(lua_State *L, lua_Debug *ar)
{
    int64_t nowTime = ox_getnowtime();
    if ((nowTime - monitorTime) >= 5000)
    {
        /*TODO::callstack*/
        luaL_error(L, "%s", "while dead loop \n");
    }
}

class CoreDD
{
public:
    CoreDD()
    {
        mTimerMgr = std::make_shared<TimerMgr>();
        mNextServiceID = 0;
        mAsyncConnector = std::make_shared<ThreadConnector>([this](sock fd, int64_t uid){
            AsyncConnectResult tmp = { fd, uid };
            mAsyncConnectResultList.Push(tmp);
            mAsyncConnectResultList.ForceSyncWrite();
            mLogicLoop.wakeup();
        });
        mAsyncConnector->startThread();
    }

    void startMonitor()
    {
        monitorTime = ox_getnowtime();
    }

    int64_t getNowUnixTime()
    {
        return ox_getnowtime();
    }

    int64_t startTimer(int delayMs, const char* callback)
    {
        int64_t id = mTimerIDCreator.claim();

        std::string cb = callback;
        Timer::WeakPtr timer = mTimerMgr->AddTimer(delayMs, [=](){
            mTimerList.erase(id);
            lua_tinker::call<void>(L, cb.c_str(), id);
        });

        mTimerList[id] = timer;

        return id;
    }

    void    removeTimer(int64_t id)
    {
        auto it = mTimerList.find(id);
        if (it != mTimerList.end())
        {
            (*it).second.lock()->Cancel();
            mTimerList.erase(it);
        }
    }

    void    closeTcpSession(int serviceID, int64_t socketID)
    {
        auto it = mServiceList.find(serviceID);
        if (it != mServiceList.end())
        {
            auto service = (*it).second;
            auto sessionIT = service->mSessions.find(socketID);
            if (sessionIT != service->mSessions.end())
            {
                service->mTcpService->disConnect(socketID);
                service->mSessions.erase(sessionIT);
            }
        }
    }

    void    sendToTcpSession(int serviceID, int64_t socketID, const char* data, int len)
    {
        auto it = mServiceList.find(serviceID);
        if (it != mServiceList.end())
        {
            auto service = (*it).second;
            auto sessionIT = service->mSessions.find(socketID);
            if (sessionIT != service->mSessions.end())
            {
                service->mTcpService->send(socketID, DataSocket::makePacket(data, len), nullptr);
            }
        }
    }

    void    addSessionToService(int serviceID, sock fd, int64_t uid)
    {
        auto it = mServiceList.find(serviceID);
        if (it != mServiceList.end())
        {
            LuaTcpService::PTR t = (*it).second;
            auto service = (*it).second->mTcpService;
            service->addDataSocket(fd, [=](int64_t id, std::string ip){
                NetMsg* msg = new NetMsg(t->mServiceID, NMT_CONNECTED, id);
                std::string uidStr = std::to_string(uid);
                msg->setData(uidStr.c_str(), uidStr.size());
                lockMsgList();
                mNetMsgList.Push(msg);
                unlockMsgList();

                mLogicLoop.wakeup();
            }, service->getDisconnectCallback(), service->getDataCallback(), false, 1024 * 1024, false);
        }
    }

    int64_t asyncConnect(const char* ip, int port, int timeout)
    {
        int64_t id = mAsyncConnectIDCreator.claim();
        mAsyncConnector->asyncConnect(ip, port, timeout, id);
        return id;
    }

    void    loop()
    {
        time_t nearMs = mTimerMgr->NearEndMs();
        mLogicLoop.loop((nearMs == 0 && mTimerMgr->IsEmpty()) ? 100 : nearMs);

        processNetMsg();
        processAsyncConnectResult();

        mTimerMgr->Schedule();
    }

    int     createTCPService()
    {
        mNextServiceID++;
        LuaTcpService::PTR luaTcpService = std::make_shared<LuaTcpService>();
        luaTcpService->mServiceID = mNextServiceID;
        mServiceList[luaTcpService->mServiceID] = luaTcpService;

        luaTcpService->mTcpService->startWorkerThread(ox_getcpunum(), [=](EventLoop& l){
            /*每帧回调函数里强制同步rwlist*/
            lockMsgList();
            mNetMsgList.ForceSyncWrite();
            unlockMsgList();

            if (mNetMsgList.SharedListSize() > 0)
            {
                mLogicLoop.wakeup();
            }
        });

        luaTcpService->mTcpService->setEnterCallback([=](int64_t id, std::string ip){
            NetMsg* msg = new NetMsg(luaTcpService->mServiceID, NMT_ENTER, id);
            lockMsgList();
            mNetMsgList.Push(msg);
            unlockMsgList();

            mLogicLoop.wakeup();
        });

        luaTcpService->mTcpService->setDisconnectCallback([=](int64_t id){
            NetMsg* msg = new NetMsg(luaTcpService->mServiceID, NMT_CLOSE, id);
            lockMsgList();
            mNetMsgList.Push(msg);
            unlockMsgList();

            mLogicLoop.wakeup();
        });

        luaTcpService->mTcpService->setDataCallback([=](int64_t id, const char* buffer, size_t len){
            NetMsg* msg = new NetMsg(luaTcpService->mServiceID, NMT_RECV_DATA, id);
            msg->setData(buffer, len);
            lockMsgList();
            mNetMsgList.Push(msg);
            unlockMsgList();

            return len;
        });

        return luaTcpService->mServiceID;
    }

    void    listen(int serviceID, const char* ip, int port)
    {
        auto it = mServiceList.find(serviceID);
        if (it != mServiceList.end())
        {
            auto service = (*it).second;
            service->mTcpService->startListen(false, ip, port, 1024 * 1024, nullptr, nullptr);
        }
    }

private:
    void    lockMsgList()
    {
        mNetMsgMutex.lock();
    }

    void    unlockMsgList()
    {
        mNetMsgMutex.unlock();
    }

    void    processNetMsg()
    {
        mNetMsgList.SyncRead(0);
        NetMsg* msg = nullptr;
        while (mNetMsgList.PopFront(&msg))
        {
            if (msg->mType == NMT_ENTER)
            {
                LuaTcpSession::PTR luaSocket = std::make_shared<LuaTcpSession>();
                mServiceList[msg->mServiceID]->mSessions[msg->mID] = luaSocket;

                lua_tinker::call<void>(L, "__on_enter__", msg->mServiceID, msg->mID);
            }
            else if (msg->mType == NMT_CLOSE)
            {
                mServiceList[msg->mServiceID]->mSessions.erase(msg->mID);
                lua_tinker::call<void>(L, "__on_close__", msg->mServiceID, msg->mID);
            }
            else if (msg->mType == NMT_RECV_DATA)
            {
                auto client = mServiceList[msg->mServiceID]->mSessions[msg->mID];
                client->recvData += msg->mData;

                int consumeLen = lua_tinker::call<int>(L, "__on_data__", msg->mServiceID, msg->mID, client->recvData, client->recvData.size());
                client->recvData.erase(0, consumeLen);
            }
            else if (msg->mType == NMT_CONNECTED)
            {
                LuaTcpSession::PTR luaSocket = std::make_shared<LuaTcpSession>();
                mServiceList[msg->mServiceID]->mSessions[msg->mID] = luaSocket;
                int64_t uid = strtoll(msg->mData.c_str(), NULL, 10);
                lua_tinker::call<void>(L, "__on_connected__", msg->mServiceID, msg->mID, uid);
            }
            else
            {
                assert(false);
            }

            delete msg;
            msg = nullptr;
        }
    }

    void    processAsyncConnectResult()
    {
        mAsyncConnectResultList.SyncRead(0);
        AsyncConnectResult result;
        while (mAsyncConnectResultList.PopFront(&result))
        {
            lua_tinker::call<void>(L, "__on_async_connectd__", (int)result.fd, result.uid);
        }
    }
private:

    std::mutex                                  mNetMsgMutex;
    MsgQueue<NetMsg*>                           mNetMsgList;

    EventLoop                                   mLogicLoop;

    IdCreator                                   mTimerIDCreator;
    TimerMgr::PTR                               mTimerMgr;
    std::unordered_map<int64_t, Timer::WeakPtr> mTimerList;

    IdCreator                                   mAsyncConnectIDCreator;
    ThreadConnector::PTR                        mAsyncConnector;
    MsgQueue<AsyncConnectResult>                mAsyncConnectResultList;

    std::unordered_map<int, LuaTcpService::PTR> mServiceList;
    int                                         mNextServiceID;

};

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cout << "Usage : luafile" << std::endl;
        exit(-1);
    }

    ox_socket_init();

    L = luaL_newstate();
    luaopen_base(L);
    luaL_openlibs(L);
    lua_tinker::init(L);

    /*lua_sethook(L, luaRuntimeCheck, LUA_MASKLINE, 0);*/

    lua_tinker::class_add<CoreDD>(L, "CoreDD");

    lua_tinker::class_def<CoreDD>(L, "startMonitor", &CoreDD::startMonitor);
    lua_tinker::class_def<CoreDD>(L, "getNowUnixTime", &CoreDD::getNowUnixTime);

    lua_tinker::class_def<CoreDD>(L, "loop", &CoreDD::loop);

    lua_tinker::class_def<CoreDD>(L, "createTCPService", &CoreDD::createTCPService);
    lua_tinker::class_def<CoreDD>(L, "listen", &CoreDD::listen);


    lua_tinker::class_def<CoreDD>(L, "startTimer", &CoreDD::startTimer);
    lua_tinker::class_def<CoreDD>(L, "removeTimer", &CoreDD::removeTimer);

    lua_tinker::class_def<CoreDD>(L, "closeTcpSession", &CoreDD::closeTcpSession);
    lua_tinker::class_def<CoreDD>(L, "sendToTcpSession", &CoreDD::sendToTcpSession);

    lua_tinker::class_def<CoreDD>(L, "addSessionToService", &CoreDD::addSessionToService);
    lua_tinker::class_def<CoreDD>(L, "asyncConnect", &CoreDD::asyncConnect);

    CoreDD coreDD;
    lua_tinker::set(L, "CoreDD", &coreDD);

    lua_tinker::dofile(L, argv[1]);

    return 0;
}
