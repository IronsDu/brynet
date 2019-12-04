#pragma once

#include <functional>
#include <thread>
#include <cstdint>
#include <memory>

#include <brynet/base/NonCopyable.hpp>
#include <brynet/net/EventLoop.hpp>

namespace brynet { namespace net { namespace detail {

    class TcpServiceDetail;

    class IOLoopData :  public brynet::base::NonCopyable, 
                        public std::enable_shared_from_this<IOLoopData>
    {
    public:
        using Ptr = std::shared_ptr<IOLoopData>;

        static  Ptr Create(EventLoop::Ptr eventLoop,
            std::shared_ptr<std::thread> ioThread)
        {
            class make_shared_enabler : public IOLoopData
            {
            public:
                make_shared_enabler(EventLoop::Ptr eventLoop, 
                    std::shared_ptr<std::thread> ioThread)
                    :
                    IOLoopData(std::move(eventLoop), std::move(ioThread))
                {}
            };

            return std::make_shared<make_shared_enabler>(std::move(eventLoop), 
                std::move(ioThread));
        }

        const EventLoop::Ptr& getEventLoop() const
        {
            return mEventLoop;
        }

    protected:
        const std::shared_ptr<std::thread>& getIOThread() const
        {
            return mIOThread;
        }

        IOLoopData(EventLoop::Ptr eventLoop, 
            std::shared_ptr<std::thread> ioThread)
            :
            mEventLoop(std::move(eventLoop)), 
            mIOThread(std::move(ioThread))
        {}
        virtual                         ~IOLoopData() = default;

        const EventLoop::Ptr            mEventLoop;

    private:
        std::shared_ptr<std::thread>    mIOThread;

        friend class TcpServiceDetail;
    };

    using IOLoopDataPtr = std::shared_ptr<IOLoopData>;

} } }