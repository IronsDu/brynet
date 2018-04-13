#include <iostream>
#include <stdexcept>
#include <brynet/net/SocketLibFunction.h>
#include <brynet/net/EventLoop.h>
#include <brynet/net/Noexcept.h>
#include <brynet/net/TCPService.h>

const static unsigned int sDefaultLoopTimeOutMS = 100;
using namespace brynet;
using namespace brynet::net;
using namespace std::chrono;

namespace brynet
{
    namespace net
    {
        union SessionId
        {
            struct
            {
                uint16_t    loopIndex;
                uint16_t    index;
                uint32_t    iid;
            }data;  

            TcpService::SESSION_TYPE id;
        };

        class IOLoopData : public brynet::NonCopyable, public std::enable_shared_from_this<IOLoopData>
        {
        public:
            typedef std::shared_ptr<IOLoopData> PTR;

        public:
            static  PTR                     Create(EventLoop::PTR eventLoop, 
                                                   std::shared_ptr<std::thread> ioThread);

        public:
            void                            send(TcpService::SESSION_TYPE id, 
                                                 const DataSocket::PACKET_PTR& packet, 
                                                 const DataSocket::PACKED_SENDED_CALLBACK& callback);
            const EventLoop::PTR&           getEventLoop() const;

        private:
            TypeIDS<DataSocket::PTR>&       getDataSockets();
            std::shared_ptr<std::thread>&   getIOThread();
            int                             incID();

        private:
            explicit                        IOLoopData(EventLoop::PTR eventLoop,
                                                       std::shared_ptr<std::thread> ioThread);

        private:
            const EventLoop::PTR            mEventLoop;
            std::shared_ptr<std::thread>    mIOThread;

            TypeIDS<DataSocket::PTR>        mDataSockets;
            int                             mNextId;

            friend class TcpService;
        };

        void IOLoopDataSend(const std::shared_ptr<IOLoopData>& ioLoopData, 
                            TcpService::SESSION_TYPE id, 
                            const DataSocket::PACKET_PTR& packet, 
                            const DataSocket::PACKED_SENDED_CALLBACK& callback)
        {
            ioLoopData->send(id, packet, callback);
        }

        const EventLoop::PTR& IOLoopDataGetEventLoop(const std::shared_ptr<IOLoopData>& ioLoopData)
        {
            return ioLoopData->getEventLoop();
        }
    }
}

TcpService::TcpService() BRYNET_NOEXCEPT
{
    static_assert(sizeof(SessionId) == sizeof(((SessionId*)nullptr)->id), 
        "sizeof SessionId must equal int64_t");
    mRunIOLoop = std::make_shared<bool>(false);
}

TcpService::~TcpService() BRYNET_NOEXCEPT
{
    stopWorkerThread();
}

void TcpService::send(SESSION_TYPE id, 
    const DataSocket::PACKET_PTR& packet, 
    const DataSocket::PACKED_SENDED_CALLBACK& callback) const
{
    union  SessionId sid;
    sid.id = id;
    auto ioLoopData = getIOLoopDataBySocketID(id);
    if (ioLoopData != nullptr)
    {
        ioLoopData->send(id, packet, callback);
    }
}

void TcpService::postShutdown(SESSION_TYPE id) const
{
    postSessionAsyncProc(id, [](DataSocket::PTR ds){
        ds->postShutdown();
    });
}

void TcpService::postDisConnect(SESSION_TYPE id) const
{
    postSessionAsyncProc(id, [](DataSocket::PTR ds){
        ds->postDisConnect();
    });
}

void TcpService::setHeartBeat(SESSION_TYPE id, 
    std::chrono::nanoseconds checktime)
{
    postSessionAsyncProc(id, [checktime](DataSocket::PTR ds){
        ds->setHeartBeat(checktime);
    });
}

void TcpService::postSessionAsyncProc(SESSION_TYPE id, 
    std::function<void(DataSocket::PTR)> callback) const
{
    union  SessionId sid;
    sid.id = id;
    auto ioLoopData = getIOLoopDataBySocketID(id);
    if (ioLoopData == nullptr)
    {
        return;
    }

    const auto& eventLoop = ioLoopData->getEventLoop();
    auto callbackCapture = std::move(callback);
    auto shared_this = shared_from_this();
    auto ioLoopDataCapture = std::move(ioLoopData);
    eventLoop->pushAsyncProc([callbackCapture, 
        sid, 
        shared_this, 
        ioLoopDataCapture](){
        DataSocket::PTR tmp = nullptr;
        if (callbackCapture != nullptr &&
            ioLoopDataCapture->getDataSockets().get(sid.data.index, tmp) &&
            tmp != nullptr)
        {
            const auto ud = brynet::net::cast<SESSION_TYPE>(tmp->getUD());
            if (ud != nullptr && *ud == sid.id)
            {
                callbackCapture(tmp);
            }
        }
    });
}

void TcpService::stopWorkerThread()
{
    std::lock_guard<std::mutex> lck(mServiceGuard);
    std::lock_guard<std::mutex> lock(mIOLoopGuard);

    *mRunIOLoop = false;

    for (const auto& v : mIOLoopDatas)
    {
        v->getEventLoop()->wakeup();
        try
        {
            if (v->getIOThread()->joinable())
            {
                v->getIOThread()->join();
            }
        }
        catch (...)
        {
        }
    }
    mIOLoopDatas.clear();
}

void TcpService::startWorkerThread(size_t threadNum, FRAME_CALLBACK callback)
{
    std::lock_guard<std::mutex> lck(mServiceGuard);
    std::lock_guard<std::mutex> lock(mIOLoopGuard);

    if (!mIOLoopDatas.empty())
    {
        return;
    }

    mRunIOLoop = std::make_shared<bool>(true);

    mIOLoopDatas.resize(threadNum);
    for (auto& v : mIOLoopDatas)
    {
        auto eventLoop = std::make_shared<EventLoop>();
        auto runIoLoop = mRunIOLoop;
        v = IOLoopData::Create(eventLoop, std::make_shared<std::thread>([callback,
            runIoLoop,
            eventLoop]() {
            while (*runIoLoop)
            {
                auto timeout = std::chrono::milliseconds(sDefaultLoopTimeOutMS);
                if (!eventLoop->getTimerMgr()->isEmpty())
                {
                    timeout = duration_cast<milliseconds>(eventLoop->getTimerMgr()->nearLeftTime());
                }
                eventLoop->loop(timeout.count());
                if (callback != nullptr)
                {
                    callback(eventLoop);
                }
            }
        }));
    }
}

void TcpService::wakeup(SESSION_TYPE id) const
{
    union  SessionId sid;
    sid.id = id;
    auto eventLoop = getEventLoopBySocketID(id);
    if (eventLoop != nullptr)
    {
        eventLoop->wakeup();
    }
}

void TcpService::wakeupAll() const
{
    std::lock_guard<std::mutex> lock(mIOLoopGuard);

    for (const auto& v : mIOLoopDatas)
    {
        v->getEventLoop()->wakeup();
    }
}

EventLoop::PTR TcpService::getRandomEventLoop()
{
    EventLoop::PTR ret;
    {
        auto randNum = rand();
        std::lock_guard<std::mutex> lock(mIOLoopGuard);
        if (!mIOLoopDatas.empty())
        {
            ret = mIOLoopDatas[randNum % mIOLoopDatas.size()]->getEventLoop();
        }
    }

    return ret;
}

TcpService::PTR TcpService::Create()
{
    struct make_shared_enabler : public TcpService {};
    return std::make_shared<make_shared_enabler>();
}

EventLoop::PTR TcpService::getEventLoopBySocketID(SESSION_TYPE id) const BRYNET_NOEXCEPT
{
    std::lock_guard<std::mutex> lock(mIOLoopGuard);

    union  SessionId sid;
    sid.id = id;
    assert(sid.data.loopIndex < mIOLoopDatas.size());
    if (sid.data.loopIndex < mIOLoopDatas.size())
    {
        return mIOLoopDatas[sid.data.loopIndex]->getEventLoop();
    }
    else
    {
        return nullptr;
    }
}

std::shared_ptr<IOLoopData> TcpService::getIOLoopDataBySocketID(SESSION_TYPE id) const BRYNET_NOEXCEPT
{
    std::lock_guard<std::mutex> lock(mIOLoopGuard);

    union  SessionId sid;
    sid.id = id;
    assert(sid.data.loopIndex < mIOLoopDatas.size());
    if (sid.data.loopIndex < mIOLoopDatas.size())
    {
        return mIOLoopDatas[sid.data.loopIndex];
    }
    else
    {
        return nullptr;
    }
}

TcpService::SESSION_TYPE TcpService::MakeID(size_t loopIndex, 
    const std::shared_ptr<IOLoopData>& loopData)
{
    union SessionId sid;
    sid.data.loopIndex = static_cast<uint16_t>(loopIndex);
    sid.data.index = static_cast<uint16_t>(loopData->getDataSockets().claimID());
    sid.data.iid = loopData->incID();

    return sid.id;
}

void TcpService::procDataSocketClose(DataSocket::PTR ds)
{
    auto ud = brynet::net::cast<SESSION_TYPE>(ds->getUD());
    if (ud != nullptr)
    {
        union SessionId sid;
        sid.id = *ud;

        mIOLoopDatas[sid.data.loopIndex]->getDataSockets().set(nullptr, sid.data.index);
        mIOLoopDatas[sid.data.loopIndex]->getDataSockets().reclaimID(sid.data.index);
    }
}

bool TcpService::helpAddChannel(DataSocket::PTR channel, 
    const std::string& ip, 
    const TcpService::ENTER_CALLBACK& enterCallback, 
    const TcpService::DISCONNECT_CALLBACK& disConnectCallback, 
    const TcpService::DATA_CALLBACK& dataCallback,
    bool forceSameThreadLoop)
{
    std::shared_ptr<IOLoopData> ioLoopData;
    size_t loopIndex = 0;
    {
        auto randNum = rand();
        std::lock_guard<std::mutex> lock(mIOLoopGuard);

        if (mIOLoopDatas.empty())
        {
            return false;
        }
        
        if (forceSameThreadLoop)
        {
            bool find = false;
            for (size_t i = 0; i < mIOLoopDatas.size(); i++)
            {
                if (mIOLoopDatas[i]->getEventLoop()->isInLoopThread())
                {
                    loopIndex = i;
                    find = true;
                    break;
                }
            }
            if (!find)
            {
                return false;
            }
        }
        else
        {
            loopIndex = randNum % mIOLoopDatas.size();
        }

        ioLoopData = mIOLoopDatas[loopIndex];
    }
    

    const auto& loop = ioLoopData->getEventLoop();
    auto loopDataCapture = std::move(ioLoopData);
    auto shared_this = shared_from_this();
    channel->setEnterCallback([ip, 
        loopIndex, 
        enterCallback, 
        disConnectCallback, 
        dataCallback, 
        shared_this,
        loopDataCapture](DataSocket::PTR dataSocket){
        auto id = shared_this->MakeID(loopIndex, loopDataCapture);
        union SessionId sid;
        sid.id = id;
        loopDataCapture->getDataSockets().set(dataSocket, sid.data.index);
        dataSocket->setUD(id);
        dataSocket->setDataCallback([dataCallback, 
            id](DataSocket::PTR ds, const char* buffer, size_t len){
            return dataCallback(id, buffer, len);
        });

        dataSocket->setDisConnectCallback([disConnectCallback, 
            id, 
            shared_this](DataSocket::PTR arg){
            shared_this->procDataSocketClose(arg);
            disConnectCallback(id);
            delete arg;
        });

        if (enterCallback != nullptr)
        {
            enterCallback(id, ip);
        }
    });

    //TODO::考虑channel 内存更安全的方式
    loop->pushAsyncProc([loop, channel](){
        if (!channel->onEnterEventLoop(std::move(loop)))
        {
            delete channel;
        }
    });

    return true;
}

struct brynet::net::TcpService::AddSocketOption::Options
{
    Options()
    {
        useSSL = false;
        forceSameThreadLoop = false;
        maxRecvBufferSize = 0;
    }

    TcpService::ENTER_CALLBACK      enterCallback;
    TcpService::DISCONNECT_CALLBACK disConnectCallback;
    TcpService::DATA_CALLBACK       dataCallback;

    SSLHelper::PTR                  sslHelper;
    bool                            useSSL;
    bool                            forceSameThreadLoop;
    size_t                          maxRecvBufferSize;
};

bool TcpService::_addDataSocket(TcpSocket::PTR socket,
    const std::vector<AddSocketOption::AddSocketOptionFunc>& optionFuncs)
{
    struct TcpService::AddSocketOption::Options options;
    for (const auto& v : optionFuncs)
    {
        if (v != nullptr)
        {
            v(options);
        }
    }

    if (options.maxRecvBufferSize <= 0)
    {
        throw std::runtime_error("buffer size is zero");
    }

    const auto isServerSide = socket->isServerSide();
    const std::string ip = socket->GetIP();

    DataSocket::PTR channel = new DataSocket(std::move(socket), options.maxRecvBufferSize);
#ifdef USE_OPENSSL
    if (options.useSSL)
    {
        if (isServerSide)
        {
            if (options.sslHelper == nullptr ||
                options.sslHelper->getOpenSSLCTX() == nullptr ||
                !channel->initAcceptSSL(options.sslHelper->getOpenSSLCTX()))
            {
                goto FAILED;
            }
        }
        else
        {
            if (!channel->initConnectSSL())
            {
                goto FAILED;
            }
        }
    }
#else
    if (options.useSSL)
    {
        goto FAILED;
    }
#endif

    if (helpAddChannel(channel,
        ip,
        options.enterCallback,
        options.disConnectCallback,
        options.dataCallback,
        options.forceSameThreadLoop))
    {
        return true;
    }

FAILED:
    if (channel != nullptr)
    {
        delete channel;
        channel = nullptr;
    }

    return false;
}

IOLoopData::PTR IOLoopData::Create(EventLoop::PTR eventLoop, std::shared_ptr<std::thread> ioThread)
{
    struct make_shared_enabler : public IOLoopData
    {
        make_shared_enabler(EventLoop::PTR eventLoop, std::shared_ptr<std::thread> ioThread) :
            IOLoopData(std::move(eventLoop), std::move(ioThread))
        {}
    };

    return std::make_shared<make_shared_enabler>(std::move(eventLoop), std::move(ioThread));
}

IOLoopData::IOLoopData(EventLoop::PTR eventLoop, std::shared_ptr<std::thread> ioThread)
    : mEventLoop(std::move(eventLoop)), mIOThread(std::move(ioThread)), mNextId(0)
{}

const EventLoop::PTR& IOLoopData::getEventLoop() const
{
    return mEventLoop;
}

brynet::TypeIDS<DataSocket::PTR>& IOLoopData::getDataSockets()
{
    return mDataSockets;
}

std::shared_ptr<std::thread>& IOLoopData::getIOThread()
{
    return mIOThread;
}

int IOLoopData::incID()
{
    return mNextId++;
}

void IOLoopData::send(TcpService::SESSION_TYPE id, 
    const DataSocket::PACKET_PTR& packet, 
    const DataSocket::PACKED_SENDED_CALLBACK& callback)
{
    union  SessionId sid;
    sid.id = id;

    if (mEventLoop->isInLoopThread())
    {
        DataSocket::PTR tmp = nullptr;
        if (mDataSockets.get(sid.data.index, tmp) &&
            tmp != nullptr)
        {
            const auto ud = brynet::net::cast<TcpService::SESSION_TYPE>(tmp->getUD());
            if (ud != nullptr && *ud == sid.id)
            {
                tmp->sendInLoop(packet, callback);
            }
        }
    }
    else
    {
        auto packetCapture = packet;
        auto callbackCapture = callback;
        auto ioLoopDataCapture = shared_from_this();
        mEventLoop->pushAsyncProc([packetCapture, 
            callbackCapture, 
            sid, 
            ioLoopDataCapture](){
            DataSocket::PTR tmp = nullptr;
            if (ioLoopDataCapture->mDataSockets.get(sid.data.index, tmp) &&
                tmp != nullptr)
            {
                const auto ud = brynet::net::cast<TcpService::SESSION_TYPE>(tmp->getUD());
                if (ud != nullptr && *ud == sid.id)
                {
                    tmp->sendInLoop(packetCapture, callbackCapture);
                }
            }
        });
    }
}

TcpService::AddSocketOption::AddSocketOptionFunc TcpService::AddSocketOption::WithEnterCallback(TcpService::ENTER_CALLBACK callback)
{
    return [=](TcpService::AddSocketOption::Options& option) {
        option.enterCallback = callback;
    };
}

TcpService::AddSocketOption::AddSocketOptionFunc TcpService::AddSocketOption::WithDisconnectCallback(TcpService::DISCONNECT_CALLBACK callback)
{
    return [=](TcpService::AddSocketOption::Options& option) {
        option.disConnectCallback = callback;
    };
}

TcpService::AddSocketOption::AddSocketOptionFunc TcpService::AddSocketOption::WithDataCallback(TcpService::DATA_CALLBACK callback)
{
    return [=](TcpService::AddSocketOption::Options& option) {
        option.dataCallback = callback;
    };
}

TcpService::AddSocketOption::AddSocketOptionFunc TcpService::AddSocketOption::WithClientSideSSL()
{
    return [=](TcpService::AddSocketOption::Options& option) {
        option.useSSL = true;
    };
}

TcpService::AddSocketOption::AddSocketOptionFunc TcpService::AddSocketOption::WithServerSideSSL(SSLHelper::PTR sslHelper)
{
    return [=](TcpService::AddSocketOption::Options& option) {
        option.sslHelper = sslHelper;
        option.useSSL = true;
    };
}

TcpService::AddSocketOption::AddSocketOptionFunc TcpService::AddSocketOption::WithMaxRecvBufferSize(size_t size)
{
    return [=](TcpService::AddSocketOption::Options& option) {
        option.maxRecvBufferSize = size;
    };
}

TcpService::AddSocketOption::AddSocketOptionFunc TcpService::AddSocketOption::WithForceSameThreadLoop(bool same)
{
    return [=](AddSocketOption::Options& option) {
        option.forceSameThreadLoop = same;
    };
}