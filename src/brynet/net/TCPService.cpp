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
        class IOLoopData : public brynet::NonCopyable, public std::enable_shared_from_this<IOLoopData>
        {
        public:
            typedef std::shared_ptr<IOLoopData> PTR;

        public:
            static  PTR                     Create(EventLoop::PTR eventLoop, 
                                                   std::shared_ptr<std::thread> ioThread);
        public:
            const EventLoop::PTR&           getEventLoop() const;

        private:
            std::shared_ptr<std::thread>&   getIOThread();

        private:
            explicit                        IOLoopData(EventLoop::PTR eventLoop,
                                                       std::shared_ptr<std::thread> ioThread);
            virtual                         ~IOLoopData() = default;

        private:
            const EventLoop::PTR            mEventLoop;
            std::shared_ptr<std::thread>    mIOThread;

            friend class TcpService;
        };
    }
}

TcpService::TcpService() BRYNET_NOEXCEPT
{
    mRunIOLoop = std::make_shared<bool>(false);
}

TcpService::~TcpService() BRYNET_NOEXCEPT
{
    stopWorkerThread();
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

EventLoop::PTR TcpService::getRandomEventLoop()
{
    const auto randNum = rand();
    std::lock_guard<std::mutex> lock(mIOLoopGuard);
    if (mIOLoopDatas.empty())
    {
        return nullptr;
    }
    return mIOLoopDatas[randNum % mIOLoopDatas.size()]->getEventLoop();
}

TcpService::PTR TcpService::Create()
{
    struct make_shared_enabler : public TcpService {};
    return std::make_shared<make_shared_enabler>();
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

    EventLoop::PTR eventLoop;
    if (options.forceSameThreadLoop)
    {
        eventLoop = getSameThreadEventLoop();
    }
    else
    {
        eventLoop = getRandomEventLoop();
    }
    if (eventLoop == nullptr)
    {
        return false;
    }

    const auto isServerSide = socket->isServerSide();
    auto dataSocket = DataSocket::Create(std::move(socket), 
        options.maxRecvBufferSize, 
        options.enterCallback,
        eventLoop);
#ifdef USE_OPENSSL
    if (options.useSSL)
    {
        if (isServerSide)
        {
            if (options.sslHelper == nullptr ||
                options.sslHelper->getOpenSSLCTX() == nullptr ||
                !dataSocket->initAcceptSSL(options.sslHelper->getOpenSSLCTX()))
            {
                return false;
            }
        }
        else
        {
            if (!dataSocket->initConnectSSL())
            {
                return false;
            }
        }
    }
#else
    if (options.useSSL)
    {
        return false;
    }
#endif
    eventLoop->pushAsyncProc([dataSocket]() {
        dataSocket->onEnterEventLoop();
    });

    return true;
}

EventLoop::PTR TcpService::getSameThreadEventLoop()
{
    std::lock_guard<std::mutex> lock(mIOLoopGuard);
    for (const auto& v : mIOLoopDatas)
    {
        if (v->getEventLoop()->isInLoopThread())
        {
            return v->getEventLoop();
        }
    }
    return nullptr;
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
    : mEventLoop(std::move(eventLoop)), mIOThread(std::move(ioThread))
{}

const EventLoop::PTR& IOLoopData::getEventLoop() const
{
    return mEventLoop;
}

std::shared_ptr<std::thread>& IOLoopData::getIOThread()
{
    return mIOThread;
}

TcpService::AddSocketOption::AddSocketOptionFunc TcpService::AddSocketOption::WithEnterCallback(TcpService::ENTER_CALLBACK callback)
{
    return [=](TcpService::AddSocketOption::Options& option) {
        option.enterCallback = callback;
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