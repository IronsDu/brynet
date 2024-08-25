#pragma once

#include <brynet/base/NonCopyable.hpp>
#include <brynet/base/WaitGroup.hpp>
#include <brynet/net/EventLoop.hpp>
#include <brynet/net/Exception.hpp>
#include <brynet/net/detail/ConnectorWorkInfo.hpp>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

namespace brynet { namespace net { namespace detail {

class AsyncConnectorDetail : public brynet::base::NonCopyable
{
protected:
    void startWorkerThread()
    {
        std::lock_guard<std::mutex> lck(mThreadGuard);

        if (mThread != nullptr)
        {
            throw std::runtime_error("connect thread already started");
        }

        mIsRun = std::make_shared<bool>(true);
        mWorkInfo = std::make_shared<detail::ConnectorWorkInfo>();
        mEventLoop = std::make_shared<EventLoop>();

        auto eventLoop = mEventLoop;
        auto workerInfo = mWorkInfo;
        auto isRun = mIsRun;

        auto wg = brynet::base::WaitGroup::Create();
        wg->add(1);
        mThread = std::make_shared<std::thread>([wg, eventLoop, workerInfo, isRun]() {
            eventLoop->bindCurrentThread();
            wg->done();

            while (*isRun)
            {
                detail::RunOnceCheckConnect(eventLoop, workerInfo);
            }

            workerInfo->causeAllFailed();
        });

        wg->wait();
    }

    void stopWorkerThread()
    {
        std::lock_guard<std::mutex> lck(mThreadGuard);

        if (mThread == nullptr)
        {
            return;
        }

        mEventLoop->runAsyncFunctor([this]() {
            *mIsRun = false;
        });

        try
        {
            if (mThread->joinable())
            {
                mThread->join();
            }
        }
        catch (std::system_error& e)
        {
            (void) e;
        }

        mEventLoop = nullptr;
        mWorkInfo = nullptr;
        mIsRun = nullptr;
        mThread = nullptr;
    }

    void asyncConnect(detail::ConnectOption option)
    {
        std::lock_guard<std::mutex> lck(mThreadGuard);

        if (option.completedCallback == nullptr && option.failedCallback == nullptr)
        {
            throw ConnectException("all callback is nullptr");
        }
        if (option.ip.empty())
        {
            throw ConnectException("addr is empty");
        }

        if (!(*mIsRun))
        {
            throw ConnectException("work thread already stop");
        }

        auto workInfo = mWorkInfo;
        auto address = detail::AsyncConnectAddr(std::move(option.ip),
                                                option.port,
                                                option.timeout,
                                                std::move(option.completedCallback),
                                                std::move(option.failedCallback),
                                                std::move(option.processCallbacks));
        mEventLoop->runAsyncFunctor([workInfo, address]() {
            workInfo->processConnect(address);
        });
    }

protected:
    AsyncConnectorDetail()
    {
        mIsRun = std::make_shared<bool>(false);
    }

    virtual ~AsyncConnectorDetail()
    {
        stopWorkerThread();
    }

private:
    std::shared_ptr<EventLoop> mEventLoop;

    std::shared_ptr<detail::ConnectorWorkInfo> mWorkInfo;
    std::shared_ptr<std::thread> mThread;
    std::mutex mThreadGuard;
    std::shared_ptr<bool> mIsRun;
};

}}}// namespace brynet::net::detail
