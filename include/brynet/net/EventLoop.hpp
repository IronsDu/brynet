#pragma once

#include <cstdint>
#include <functional>
#include <vector>
#include <mutex>
#include <memory>
#include <atomic>
#include <unordered_map>
#include <cassert>
#include <algorithm>

#include <brynet/base/Timer.hpp>
#include <brynet/base/NonCopyable.hpp>
#include <brynet/base/Noexcept.hpp>
#include <brynet/net/SocketLibFunction.hpp>
#include <brynet/net/CurrentThread.hpp>

#include <brynet/net/Channel.hpp>
#include <brynet/net/Socket.hpp>
#include <brynet/net/Exception.hpp>
#include <brynet/net/detail/WakeupChannel.hpp>

namespace brynet { namespace net {

    class Channel;
    class TcpConnection;
    using TcpConnectionPtr = std::shared_ptr<TcpConnection>;

    class EventLoop : public brynet::base::NonCopyable
    {
    public:
        using Ptr = std::shared_ptr<EventLoop>;
        using UserFunctor = std::function<void(void)>;

    public:
        EventLoop()
            BRYNET_NOEXCEPT
            :
#ifdef BRYNET_PLATFORM_WINDOWS
            mIOCP(CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 1)),
            mWakeupChannel(std::make_unique<detail::WakeupChannel>(mIOCP))
#elif defined BRYNET_PLATFORM_LINUX
            mEpollFd(epoll_create(1))
#elif defined BRYNET_PLATFORM_DARWIN
            mKqueueFd(kqueue())
#endif
        {
#ifdef BRYNET_PLATFORM_WINDOWS
            mPGetQueuedCompletionStatusEx = NULL;
            auto kernel32_module = GetModuleHandleA("kernel32.dll");
            if (kernel32_module != NULL) {
                mPGetQueuedCompletionStatusEx = reinterpret_cast<sGetQueuedCompletionStatusEx>(GetProcAddress(
                    kernel32_module,
                    "GetQueuedCompletionStatusEx"));
                FreeLibrary(kernel32_module);
            }
#elif defined BRYNET_PLATFORM_LINUX
            auto eventfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
            mWakeupChannel.reset(new detail::WakeupChannel(eventfd));
            linkChannel(eventfd, mWakeupChannel.get());
#elif defined BRYNET_PLATFORM_DARWIN
            const int NOTIFY_IDENT = 42; // Magic number we use for our filter ID.
            mWakeupChannel.reset(new detail::WakeupChannel(mKqueueFd, NOTIFY_IDENT));
            //Add user event
            struct kevent ev;
            EV_SET(&ev, NOTIFY_IDENT, EVFILT_USER, EV_ADD | EV_CLEAR, 0, 0, NULL);

            struct timespec timeout = { 0, 0 };
            kevent(mKqueueFd, &ev, 1, NULL, 0, &timeout);
#endif

            mIsAlreadyPostWakeup = false;
            mIsInBlock = true;

            reallocEventSize(1024);
            mSelfThreadID = -1;
            mTimer = std::make_shared<brynet::base::TimerMgr>();
        }

        virtual ~EventLoop() BRYNET_NOEXCEPT
        {
#ifdef BRYNET_PLATFORM_WINDOWS
            CloseHandle(mIOCP);
            mIOCP = INVALID_HANDLE_VALUE;
#elif defined BRYNET_PLATFORM_LINUX
            close(mEpollFd);
            mEpollFd = -1;
#elif defined BRYNET_PLATFORM_DARWIN
            close(mKqueueFd);
            mKqueueFd = -1;
#endif
        }

        void                            loop(int64_t milliseconds)
        {
            tryInitThreadID();

#ifndef NDEBUG
            assert(isInLoopThread());
#endif
            if (!isInLoopThread())
            {
                throw BrynetCommonException("only loop in io thread");
            }

            if (!mAfterLoopFunctors.empty())
            {
                milliseconds = 0;
            }

#ifdef BRYNET_PLATFORM_WINDOWS
            ULONG numComplete = 0;
            if (mPGetQueuedCompletionStatusEx != nullptr)
            {
                if (!mPGetQueuedCompletionStatusEx(mIOCP,
                    mEventEntries.data(),
                    static_cast<ULONG>(mEventEntries.size()),
                    &numComplete,
                    static_cast<DWORD>(milliseconds),
                    false))
                {
                    numComplete = 0;
                }
            }
            else
            {
                for (auto& e : mEventEntries)
                {
                    const auto timeout = (numComplete == 0) ? static_cast<DWORD>(milliseconds) : 0;
                    /* don't check the return value of GQCS */
                    GetQueuedCompletionStatus(mIOCP,
                        &e.dwNumberOfBytesTransferred,
                        &e.lpCompletionKey,
                        &e.lpOverlapped,
                        timeout);
                    if (e.lpOverlapped == nullptr)
                    {
                        break;
                    }
                    ++numComplete;
                }
            }

            mIsInBlock = false;

            for (ULONG i = 0; i < numComplete; ++i)
            {
                auto channel = (Channel*)mEventEntries[i].lpCompletionKey;
                assert(channel != nullptr);
                const auto ovl = reinterpret_cast<const port::Win::OverlappedExt*>(mEventEntries[i].lpOverlapped);
                if (ovl->OP == port::Win::OverlappedType::OverlappedRecv)
                {
                    channel->canRecv();
                }
                else if (ovl->OP == port::Win::OverlappedType::OverlappedSend)
                {
                    channel->canSend();
                }
                else
                {
                    assert(false);
                }
            }
#elif defined BRYNET_PLATFORM_LINUX
            int numComplete = epoll_wait(mEpollFd, mEventEntries.data(), mEventEntries.size(), milliseconds);

            mIsInBlock = false;

            for (int i = 0; i < numComplete; ++i)
            {
                auto    channel = (Channel*)(mEventEntries[i].data.ptr);
                auto    event_data = mEventEntries[i].events;

                if (event_data & EPOLLRDHUP)
                {
                    channel->canRecv();
                    channel->onClose();
                    continue;
                }

                if (event_data & EPOLLIN)
                {
                    channel->canRecv();
                }

                if (event_data & EPOLLOUT)
                {
                    channel->canSend();
                }
            }
#elif defined BRYNET_PLATFORM_DARWIN
            struct timespec timeout = { milliseconds / 1000, (milliseconds % 1000) * 1000 * 1000 };
            int numComplete = kevent(mKqueueFd, NULL, 0, mEventEntries.data(), mEventEntries.size(), &timeout);

            mIsInBlock = false;

            for (int i = 0; i < numComplete; ++i)
            {
                auto channel = (Channel*)(mEventEntries[i].udata);
                const struct kevent& event = mEventEntries[i];

                if (event.filter == EVFILT_USER)
                {
                    continue;
                }

                if (event.filter == EVFILT_READ)
                {
                    channel->canRecv();
                }

                if (event.filter == EVFILT_WRITE)
                {
                    channel->canSend();
                }
            }
#endif

            mIsAlreadyPostWakeup = false;
            mIsInBlock = true;

            processAsyncFunctors();
            processAfterLoopFunctors();

            if (static_cast<size_t>(numComplete) == mEventEntries.size())
            {
                reallocEventSize(mEventEntries.size() + 128);
            }

            mTimer->schedule();
        }

        // loop指定毫秒数,但如果定时器不为空,则loop时间为当前最近定时器的剩余时间和milliseconds的较小值
        void                            loopCompareNearTimer(int64_t milliseconds)
        {
            tryInitThreadID();

#ifndef NDEBUG
            assert(isInLoopThread());
#endif
            if (!isInLoopThread())
            {
                throw BrynetCommonException("only loop in IO thread");
            }

            if (!mTimer->isEmpty())
            {
                auto nearTimeout = std::chrono::duration_cast<std::chrono::milliseconds>(mTimer->nearLeftTime());
                milliseconds = std::min<int64_t>(milliseconds, nearTimeout.count());
            }

            loop(milliseconds);
        }

        // 返回true表示实际发生了wakeup所需的操作(此返回值不代表接口本身操作成功与否,因为此函数永远成功)
        bool                            wakeup()
        {
            if (!isInLoopThread() && mIsInBlock && !mIsAlreadyPostWakeup.exchange(true))
            {
                return mWakeupChannel->wakeup();
            }

            return false;
        }

        void                            runAsyncFunctor(UserFunctor&& f)
        {
            if (isInLoopThread())
            {
                f();
            }
            else
            {
                pushAsyncFunctor(std::move(f));
                wakeup();
            }
        }
        void                            runFunctorAfterLoop(UserFunctor&& f)
        {
            assert(isInLoopThread());
            if (!isInLoopThread())
            {
                throw BrynetCommonException("only push after functor in io thread");
            }

            mAfterLoopFunctors.emplace_back(std::move(f));
        }
        brynet::base::Timer::WeakPtr   runAfter(std::chrono::nanoseconds timeout, UserFunctor&& callback)
        {
            auto timer = std::make_shared<brynet::base::Timer>(
                std::chrono::steady_clock::now(),
                std::chrono::nanoseconds(timeout),
                std::move(callback));

            if (isInLoopThread())
            {
                mTimer->addTimer(timer);
            }
            else
            {
                auto timerMgr = mTimer;
                runAsyncFunctor([timerMgr, timer]() {
                    timerMgr->addTimer(timer);
                    });
            }

            return timer;
        }

        inline bool                     isInLoopThread() const
        {
            return mSelfThreadID == current_thread::tid();
        }

    private:
        void                            reallocEventSize(size_t size)
        {
            mEventEntries.resize(size);
        }

        void                            processAfterLoopFunctors()
        {
            mCopyAfterLoopFunctors.swap(mAfterLoopFunctors);
            for (const auto& x : mCopyAfterLoopFunctors)
            {
                x();
            }
            mCopyAfterLoopFunctors.clear();
        }
        void                            processAsyncFunctors()
        {
            swapAsyncFunctors();

            for (const auto& x : mCopyAsyncFunctors)
            {
                x();
            }
            mCopyAsyncFunctors.clear();
        }
        void                            swapAsyncFunctors()
        {
            std::lock_guard<std::mutex> lck(mAsyncFunctorsMutex);
            assert(mCopyAsyncFunctors.empty());
            mCopyAsyncFunctors.swap(mAsyncFunctors);
        }
        void                            pushAsyncFunctor(UserFunctor&& f)
        {
            std::lock_guard<std::mutex> lck(mAsyncFunctorsMutex);
            mAsyncFunctors.emplace_back(std::move(f));
        }

#ifdef BRYNET_PLATFORM_LINUX
        int                             getEpollHandle() const
        {
            return mEpollFd;
        }
#elif defined BRYNET_PLATFORM_DARWIN
        int                             getKqueueHandle() const
        {
            return mKqueueFd;
        }
#endif
        bool                            linkChannel(BrynetSocketFD fd, const Channel* ptr) BRYNET_NOEXCEPT
        {
#ifdef BRYNET_PLATFORM_WINDOWS
            return CreateIoCompletionPort((HANDLE)fd, mIOCP, (ULONG_PTR)ptr, 0) != nullptr;
#elif defined BRYNET_PLATFORM_LINUX
            struct epoll_event ev = { 0, { nullptr } };
            ev.events = EPOLLET | EPOLLIN | EPOLLOUT | EPOLLRDHUP;
            ev.data.ptr = (void*)ptr;
            return epoll_ctl(mEpollFd, EPOLL_CTL_ADD, fd, &ev) == 0;
#elif defined BRYNET_PLATFORM_DARWIN
            struct kevent ev[2];
            memset(&ev, 0, sizeof(ev));
            int n = 0;
            EV_SET(&ev[n++], fd, EVFILT_READ, EV_ADD | EV_CLEAR, NOTE_TRIGGER, 0, (void*)ptr);
            EV_SET(&ev[n++], fd, EVFILT_WRITE, EV_ADD | EV_CLEAR, NOTE_TRIGGER, 0, (void*)ptr);

            struct timespec now = { 0, 0 };
            return kevent(mKqueueFd, ev, n, NULL, 0, &now) == 0;
#endif
        }
        TcpConnectionPtr                getTcpConnection(BrynetSocketFD fd)
        {
            auto it = mTcpConnections.find(fd);
            if (it != mTcpConnections.end())
            {
                return (*it).second;
            }
            return nullptr;
        }
        void                            addTcpConnection(BrynetSocketFD fd, TcpConnectionPtr tcpConnection)
        {
            mTcpConnections[fd] = std::move(tcpConnection);
        }
        void                            removeTcpConnection(BrynetSocketFD fd)
        {
            mTcpConnections.erase(fd);
        }
        void                            tryInitThreadID()
        {
            std::call_once(mOnceInitThreadID, [this]() {
                mSelfThreadID = current_thread::tid();
                });
        }

    private:

#ifdef BRYNET_PLATFORM_WINDOWS
        std::vector<OVERLAPPED_ENTRY>   mEventEntries;

        typedef BOOL(WINAPI *sGetQueuedCompletionStatusEx) (HANDLE, LPOVERLAPPED_ENTRY, ULONG, PULONG, DWORD, BOOL);
        sGetQueuedCompletionStatusEx    mPGetQueuedCompletionStatusEx;
        HANDLE                          mIOCP;
#elif defined BRYNET_PLATFORM_LINUX
        std::vector<epoll_event>        mEventEntries;
        int                             mEpollFd;
#elif defined BRYNET_PLATFORM_DARWIN
        std::vector<struct kevent>      mEventEntries;
        int                             mKqueueFd;
#endif
        std::unique_ptr<detail::WakeupChannel>  mWakeupChannel;

        std::atomic_bool                mIsInBlock;
        std::atomic_bool                mIsAlreadyPostWakeup;

        std::mutex                      mAsyncFunctorsMutex;
        std::vector<UserFunctor>        mAsyncFunctors;
        std::vector<UserFunctor>        mCopyAsyncFunctors;

        std::vector<UserFunctor>        mAfterLoopFunctors;
        std::vector<UserFunctor>        mCopyAfterLoopFunctors;

        std::once_flag                  mOnceInitThreadID;
        current_thread::THREAD_ID_TYPE  mSelfThreadID;

        brynet::base::TimerMgr::Ptr    mTimer;
        std::unordered_map<BrynetSocketFD, TcpConnectionPtr> mTcpConnections;

        friend class TcpConnection;
    };

} }