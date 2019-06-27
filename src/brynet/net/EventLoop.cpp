#include <cassert>
#include <algorithm>

#include <brynet/net/Channel.h>
#include <brynet/net/Socket.h>
#include <brynet/net/Exception.h>
#include <brynet/net/EventLoop.h>

using namespace brynet::net;

namespace brynet { namespace net {

#ifdef PLATFORM_WINDOWS
    class WakeupChannel final : public Channel, public utils::NonCopyable
    {
    public:
        explicit WakeupChannel(HANDLE iocp) : mIOCP(iocp), mWakeupOvl(port::Win::OverlappedType::OverlappedRecv)
        {
        }

        bool    wakeup() BRYNET_NOEXCEPT
        {
            return PostQueuedCompletionStatus(mIOCP, 0, reinterpret_cast<ULONG_PTR>(this), &mWakeupOvl.base);
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
#elif defined PLATFORM_LINUX
    class WakeupChannel final : public Channel, public utils::NonCopyable
    {
    public:
        explicit WakeupChannel(sock fd) : mUniqueFd(fd)
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

#elif defined PLATFORM_DARWIN
    class WakeupChannel final : public Channel, public utils::NonCopyable
    {
    public:
        explicit WakeupChannel(int kqueuefd, int ident) : mKqueueFd(kqueuefd), mUserEvent(ident)
        {
        }

        bool wakeup()
        {
            struct kevent ev;
            EV_SET(&ev, mUserEvent, EVFILT_USER, 0, NOTE_TRIGGER, 0, NULL);

            struct timespec timeout = {0, 0};
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
    EventLoop::EventLoop()
        BRYNET_NOEXCEPT
        :
#ifdef PLATFORM_WINDOWS
    mIOCP(CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 1)), 
    mWakeupChannel(std::make_unique<WakeupChannel>(mIOCP))
#elif defined PLATFORM_LINUX
    mEpollFd(epoll_create(1))
#elif defined PLATFORM_DARWIN
    mKqueueFd(kqueue())
#endif
    {
#ifdef PLATFORM_WINDOWS
        mPGetQueuedCompletionStatusEx = NULL;
        auto kernel32_module = GetModuleHandleA("kernel32.dll");
        if (kernel32_module != NULL) {
            mPGetQueuedCompletionStatusEx = reinterpret_cast<sGetQueuedCompletionStatusEx>(GetProcAddress(
                kernel32_module,
                "GetQueuedCompletionStatusEx"));
            FreeLibrary(kernel32_module);
        }
#elif defined PLATFORM_LINUX
        auto eventfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
        mWakeupChannel.reset(new WakeupChannel(eventfd));
        linkChannel(eventfd, mWakeupChannel.get());
#elif defined PLATFORM_DARWIN
        const int NOTIFY_IDENT = 42; // Magic number we use for our filter ID.
        mWakeupChannel.reset(new WakeupChannel(mKqueueFd, NOTIFY_IDENT));
        //Add user event
        struct kevent ev;
        EV_SET(&ev, NOTIFY_IDENT, EVFILT_USER, EV_ADD | EV_CLEAR, 0, 0, NULL);

        struct timespec timeout = {0, 0};
        kevent(mKqueueFd, &ev, 1, NULL, 0, &timeout);
#endif

        mIsAlreadyPostWakeup = false;
        mIsInBlock = true;

        reallocEventSize(1024);
        mSelfThreadID = -1;
        mTimer = std::make_shared<timer::TimerMgr>();
    }


    EventLoop::~EventLoop() BRYNET_NOEXCEPT
    {
#ifdef PLATFORM_WINDOWS
        CloseHandle(mIOCP);
        mIOCP = INVALID_HANDLE_VALUE;
#elif defined PLATFORM_LINUX
        close(mEpollFd);
        mEpollFd = -1;
#elif defined PLATFORM_DARWIN
        close(mKqueueFd);
        mKqueueFd = -1;
#endif
    }

    brynet::timer::Timer::WeakPtr EventLoop::runAfter(std::chrono::nanoseconds timeout, UserFunctor&& callback)
    {
        auto timer = std::make_shared<brynet::timer::Timer>(
            std::chrono::steady_clock::now(),
            std::chrono::nanoseconds(timeout),
            std::forward<UserFunctor>(callback));

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

    inline void EventLoop::tryInitThreadID()
    {
        std::call_once(mOnceInitThreadID, [this]() {
            mSelfThreadID = current_thread::tid();
        });
    }

    void EventLoop::loop(int64_t milliseconds)
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

#ifdef PLATFORM_WINDOWS
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
#elif defined PLATFORM_LINUX
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
#elif defined PLATFORM_DARWIN
        struct timespec timeout = { milliseconds / 1000, (milliseconds % 1000) * 1000 * 1000 };
        int numComplete = kevent(mKqueueFd, NULL, 0, mEventEntries.data(), mEventEntries.size(), &timeout);

        mIsInBlock = false;

        for (int i = 0; i < numComplete; ++i)
        {
            auto channel = (Channel *)(mEventEntries[i].udata);
            const struct kevent &event = mEventEntries[i];

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

    void EventLoop::loopCompareNearTimer(int64_t milliseconds)
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

    void EventLoop::processAfterLoopFunctors()
    {
        mCopyAfterLoopFunctors.swap(mAfterLoopFunctors);
        for (const auto& x : mCopyAfterLoopFunctors)
        {
            x();
        }
        mCopyAfterLoopFunctors.clear();
    }

    void EventLoop::swapAsyncFunctors()
    {
        std::lock_guard<std::mutex> lck(mAsyncFunctorsMutex);
        assert(mCopyAsyncFunctors.empty());
        mCopyAsyncFunctors.swap(mAsyncFunctors);
    }

    void EventLoop::processAsyncFunctors()
    {
        swapAsyncFunctors();

        for (const auto& x : mCopyAsyncFunctors)
        {
            x();
        }
        mCopyAsyncFunctors.clear();
    }

    bool EventLoop::wakeup()
    {
        if (!isInLoopThread() && mIsInBlock && !mIsAlreadyPostWakeup.exchange(true))
        {
            return mWakeupChannel->wakeup();
        }

        return false;
    }

    bool EventLoop::linkChannel(sock fd, const Channel* ptr) BRYNET_NOEXCEPT
    {
#ifdef PLATFORM_WINDOWS
        return CreateIoCompletionPort((HANDLE)fd, mIOCP, (ULONG_PTR)ptr, 0) != nullptr;
#elif defined PLATFORM_LINUX
        struct epoll_event ev = { 0, { nullptr } };
        ev.events = EPOLLET | EPOLLIN | EPOLLRDHUP;
        ev.data.ptr = (void*)ptr;
        return epoll_ctl(mEpollFd, EPOLL_CTL_ADD, fd, &ev) == 0;
#elif defined PLATFORM_DARWIN
        struct kevent ev;
        memset(&ev, 0, sizeof(ev));
        EV_SET(&ev, fd, EVFILT_READ, EV_ADD | EV_ENABLE, NOTE_TRIGGER, 0, (void *)ptr);

        struct timespec now = { 0, 0 };
        return kevent(mKqueueFd, &ev, 1, NULL, 0, &now) == 0;
#endif
    }

    TcpConnectionPtr EventLoop::getTcpConnection(sock fd)
    {
        auto it = mTcpConnections.find(fd);
        if (it != mTcpConnections.end())
        {
            return (*it).second;
        }
        return nullptr;
    }

    void EventLoop::addTcpConnection(sock fd, TcpConnectionPtr tcpConnection)
    {
        mTcpConnections[fd] = std::move(tcpConnection);
    }

    void EventLoop::removeTcpConnection(sock fd)
    {
        mTcpConnections.erase(fd);
    }

    void EventLoop::pushAsyncFunctor(UserFunctor&& f)
    {
        std::lock_guard<std::mutex> lck(mAsyncFunctorsMutex);
        mAsyncFunctors.emplace_back(std::forward<UserFunctor>(f));
    }

    void EventLoop::runAsyncFunctor(UserFunctor&& f)
    {
        if (isInLoopThread())
        {
            f();
        }
        else
        {
            pushAsyncFunctor(std::forward<UserFunctor>(f));
            wakeup();
        }
    }

    void EventLoop::runFunctorAfterLoop(UserFunctor&& f)
    {
        assert(isInLoopThread());
        if (!isInLoopThread())
        {
            throw BrynetCommonException("only push after functor in io thread");
        }

        mAfterLoopFunctors.emplace_back(std::forward<UserFunctor>(f));
    }

#ifdef PLATFORM_LINUX
    int EventLoop::getEpollHandle() const
    {
        return mEpollFd;
    }
#elif defined PLATFORM_DARWIN
    int EventLoop::getKqueueHandle() const
    {
        return mKqueueFd;
    }
#endif

    void EventLoop::reallocEventSize(size_t size)
    {
        mEventEntries.resize(size);
    }

} }
