﻿#pragma once

#include <brynet/base/Noexcept.hpp>
#include <brynet/base/NonCopyable.hpp>
#include <brynet/base/WaitGroup.hpp>
#include <brynet/net/SSLHelper.hpp>
#include <brynet/net/Socket.hpp>
#include <brynet/net/TcpConnection.hpp>
#include <brynet/net/detail/ConnectionOption.hpp>
#include <brynet/net/detail/IOLoopData.hpp>
#include <functional>
#include <memory>
#include <random>
#include <thread>
#include <vector>

namespace brynet { namespace net { namespace detail {

static void HelperAddTcpConnection(const EventLoop::Ptr& eventLoop, TcpSocket::Ptr socket, ConnectionOption option)
{
    if (eventLoop == nullptr)
    {
        throw BrynetCommonException("event loop is null");
    }
    if (option.maxRecvBufferSize <= 0)
    {
        throw BrynetCommonException("buffer size is zero");
    }

    auto wrapperEnterCallback = [option](const TcpConnection::Ptr& tcpConnection) {
        for (const auto& callback : option.enterCallback)
        {
            callback(tcpConnection);
        }
    };

    if (option.useSSL && option.sslHelper == nullptr)
    {
        option.sslHelper = SSLHelper::Create();
    }

    TcpConnection::Create(std::move(socket),
                          option.maxRecvBufferSize,
                          wrapperEnterCallback,
                          eventLoop,
                          option.sslHelper,
                          option.enterFailedCallback);
}

class TcpServiceDetail : public brynet::base::NonCopyable
{
protected:
    using FrameCallback = std::function<void(const EventLoop::Ptr&)>;
    const static unsigned int sDefaultLoopTimeOutMS = 100;

    TcpServiceDetail() BRYNET_NOEXCEPT
        : mRandom(static_cast<unsigned int>(
                  std::chrono::system_clock::now().time_since_epoch().count()))
    {
        mRunIOLoop = std::make_shared<bool>(false);
    }

    virtual ~TcpServiceDetail() BRYNET_NOEXCEPT
    {
        stopWorkerThread();
    }

    std::vector<brynet::net::EventLoop::Ptr> startWorkerThread(size_t threadNum,
                                                               FrameCallback callback = nullptr)
    {
        if (threadNum == 0)
        {
            throw std::runtime_error("thread num is zero");
        }

        std::vector<brynet::net::EventLoop::Ptr> eventLoops;

        std::lock_guard<std::mutex> lck(mServiceGuard);
        std::lock_guard<std::mutex> lock(mIOLoopGuard);

        if (!mIOLoopDatas.empty())
        {
            throw std::runtime_error("worker thread already started");
        }

        mRunIOLoop = std::make_shared<bool>(true);

        mIOLoopDatas.resize(threadNum);
        auto wg = brynet::base::WaitGroup::Create();
        for (auto& v : mIOLoopDatas)
        {
            auto eventLoop = std::make_shared<EventLoop>();
            eventLoops.push_back(eventLoop);

            auto runIoLoop = mRunIOLoop;
            wg->add(1);
            v = IOLoopData::Create(eventLoop,
                                   std::make_shared<std::thread>(
                                           [wg, callback, runIoLoop, eventLoop]() {
                                               eventLoop->bindCurrentThread();
                                               wg->done();

                                               while (*runIoLoop)
                                               {
                                                   eventLoop->loopCompareNearTimer(sDefaultLoopTimeOutMS);
                                                   if (callback != nullptr)
                                                   {
                                                       callback(eventLoop);
                                                   }
                                               }
                                           }));
        }
        wg->wait();

        return eventLoops;
    }

    void stopWorkerThread()
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
            catch (std::system_error& e)
            {
                (void) e;
            }
        }
        mIOLoopDatas.clear();
    }

    void addTcpConnection(TcpSocket::Ptr socket, ConnectionOption option)
    {
        EventLoop::Ptr eventLoop;
        if (option.forceSameThreadLoop)
        {
            eventLoop = getSameThreadEventLoop();
        }
        else
        {
            eventLoop = getRandomEventLoop();
        }
        return HelperAddTcpConnection(eventLoop, std::move(socket), std::move(option));
    }

    EventLoop::Ptr getRandomEventLoop()
    {
        std::lock_guard<std::mutex> lock(mIOLoopGuard);

        if (mIOLoopDatas.empty())
        {
            return nullptr;
        }
        else
        {
            return mIOLoopDatas[mRandom() % mIOLoopDatas.size()]->getEventLoop();
        }
    }

    EventLoop::Ptr getSameThreadEventLoop()
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

private:
    std::vector<IOLoopDataPtr> mIOLoopDatas;
    mutable std::mutex mIOLoopGuard;
    std::shared_ptr<bool> mRunIOLoop;

    std::mutex mServiceGuard;
    std::mt19937 mRandom;
};

}}}// namespace brynet::net::detail
