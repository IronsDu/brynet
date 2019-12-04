#pragma once

#include <cstdint>
#include <functional>
#include <vector>
#include <mutex>
#include <memory>
#include <atomic>
#include <cassert>

#include <brynet/base/NonCopyable.hpp>
#include <brynet/net/SocketLibFunction.hpp>
#ifdef BRYNET_PLATFORM_WINDOWS
#include <brynet/net/port/Win.hpp>
#endif

#include <brynet/net/Channel.hpp>
#include <brynet/net/Socket.hpp>

namespace brynet { namespace net { namespace detail {

#ifdef BRYNET_PLATFORM_WINDOWS
    class WakeupChannel final : public Channel, public brynet::base::NonCopyable
    {
    public:
        explicit WakeupChannel(HANDLE iocp) 
            :
            mIOCP(iocp), 
            mWakeupOvl(port::Win::OverlappedType::OverlappedRecv)
        {
        }

        bool    wakeup() BRYNET_NOEXCEPT
        {
            return PostQueuedCompletionStatus(mIOCP, 
                0, 
                reinterpret_cast<ULONG_PTR>(this), 
                &mWakeupOvl.base);
        }

    private:
        void    canRecv() BRYNET_NOEXCEPT override
        {
            ;
        }

        void    canSend() BRYNET_NOEXCEPT override
        {
            ;
        }

        void    onClose() BRYNET_NOEXCEPT override
        {
            ;
        }

        HANDLE                      mIOCP;
        port::Win::OverlappedExt    mWakeupOvl;
    };
#elif defined BRYNET_PLATFORM_LINUX
    class WakeupChannel final : public Channel, public brynet::base::NonCopyable
    {
    public:
        explicit WakeupChannel(BrynetSocketFD fd) : mUniqueFd(fd)
        {
        }

        bool    wakeup()
        {
            uint64_t one = 1;
            return write(mUniqueFd.getFD(), &one, sizeof one) > 0;
        }

    private:
        void    canRecv() override
        {
            char temp[1024 * 10];
            while (true)
            {
                auto n = read(mUniqueFd.getFD(), temp, sizeof(temp));
                if (n == -1 || static_cast<size_t>(n) < sizeof(temp))
                {
                    break;
                }
            }
        }

        void    canSend() override
        {
        }

        void    onClose() override
        {
        }

    private:
        UniqueFd    mUniqueFd;
    };

#elif defined BRYNET_PLATFORM_DARWIN
    class WakeupChannel final : public Channel, public brynet::base::NonCopyable
    {
    public:
        explicit WakeupChannel(int kqueuefd, int ident) 
            : 
            mKqueueFd(kqueuefd), 
            mUserEvent(ident)
        {
        }

        bool wakeup()
        {
            struct kevent ev;
            EV_SET(&ev, mUserEvent, EVFILT_USER, 0, NOTE_TRIGGER, 0, NULL);

            struct timespec timeout = { 0, 0 };
            return kevent(mKqueueFd, &ev, 1, NULL, 0, &timeout) == 0;
        }

    private:
        void canRecv() override
        {
        }

        void canSend() override
        {
        }

        void onClose() override
        {
        }

    private:
        int    mKqueueFd;
        int    mUserEvent;
    };
#endif

} } }