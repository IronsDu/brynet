#pragma once

#include <functional>
#include <memory>
#include <cassert>
#include <set>
#include <map>
#include <thread>

#include <brynet/base/NonCopyable.hpp>
#include <brynet/base/CPP_VERSION.hpp>
#include <brynet/base/Any.hpp>
#include <brynet/base/Noexcept.hpp>
#include <brynet/net/SocketLibFunction.hpp>
#include <brynet/net/Poller.hpp>
#include <brynet/net/Exception.hpp>
#include <brynet/net/EventLoop.hpp>
#include <brynet/net/Socket.hpp>
#include <brynet/net/detail/ConnectorWorkInfo.hpp>

#ifdef BRYNET_HAVE_LANG_CXX17
#include <shared_mutex>
#else
#include <mutex>
#endif

namespace brynet { namespace net { namespace detail {

    class AsyncConnectorDetail :  public brynet::base::NonCopyable
    {
    protected:
        void    startWorkerThread()
        {
#ifdef BRYNET_HAVE_LANG_CXX17
            std::lock_guard<std::shared_mutex> lck(mThreadGuard);
#else
            std::lock_guard<std::mutex> lck(mThreadGuard);
#endif

            if (mThread != nullptr)
            {
                return;
            }

            mIsRun = std::make_shared<bool>(true);
            mWorkInfo = std::make_shared<detail::ConnectorWorkInfo>();
            mEventLoop = std::make_shared<EventLoop>();

            auto eventLoop = mEventLoop;
            auto workerInfo = mWorkInfo;
            auto isRun = mIsRun;

            mThread = std::make_shared<std::thread>([eventLoop, workerInfo, isRun]() {
                while (*isRun)
                {
                    detail::RunOnceCheckConnect(eventLoop, workerInfo);
                }

                workerInfo->causeAllFailed();
                });
        }

        void    stopWorkerThread()
        {
#ifdef BRYNET_HAVE_LANG_CXX17
            std::lock_guard<std::shared_mutex> lck(mThreadGuard);
#else
            std::lock_guard<std::mutex> lck(mThreadGuard);
#endif

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
            catch (std::system_error & e)
            {
                (void)e;
            }

            mEventLoop = nullptr;
            mWorkInfo = nullptr;
            mIsRun = nullptr;
            mThread = nullptr;
        }

        void    asyncConnect(const std::vector<detail::ConnectOptionFunc>& options)
        {
#ifdef BRYNET_HAVE_LANG_CXX17
            std::shared_lock<std::shared_mutex> lck(mThreadGuard);
#else
            std::lock_guard<std::mutex> lck(mThreadGuard);
#endif
            detail::ConnectOptionsInfo option;
            for (const auto& func : options)
            {
                func(option);
            }

            if (option.completedCallback == nullptr && option.faledCallback == nullptr)
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
                std::move(option.faledCallback),
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
        std::shared_ptr<EventLoop>          mEventLoop;

        std::shared_ptr<detail::ConnectorWorkInfo>  mWorkInfo;
        std::shared_ptr<std::thread>        mThread;
#ifdef BRYNET_HAVE_LANG_CXX17
        std::shared_mutex                   mThreadGuard;
#else
        std::mutex                          mThreadGuard;
#endif
        std::shared_ptr<bool>               mIsRun;
    };

} } }