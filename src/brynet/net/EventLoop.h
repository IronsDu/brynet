#pragma once

#include <cstdint>
#include <functional>
#include <vector>
#include <mutex>
#include <memory>
#include <atomic>
#include <unordered_map>

#include <brynet/net/CurrentThread.h>
#include <brynet/net/SocketLibFunction.h>
#include <brynet/timer/Timer.h>
#include <brynet/utils/NonCopyable.h>
#include <brynet/net/Noexcept.h>
#if defined PLATFORM_WINDOWS
#include <brynet/net/port/Win.h>
#endif

namespace brynet { namespace net {

    using namespace std::chrono;
    using namespace brynet::timer;

    class Channel;
    class TcpConnection;
    using TcpConnectionPtr = std::shared_ptr<TcpConnection>;
    class WakeupChannel;

    class EventLoop : public utils::NonCopyable
    {
    public:
        using Ptr = std::shared_ptr<EventLoop>;
        using UserFunctor = std::function<void(void)>;

    public:
        EventLoop() BRYNET_NOEXCEPT;
        virtual ~EventLoop() BRYNET_NOEXCEPT;

        void                            loop(int64_t milliseconds);
        // loop指定毫秒数,但如果定时器不为空,则loop时间为当前最近定时器的剩余时间和milliseconds的较小值
        void                            loopCompareNearTimer(int64_t milliseconds);
        // 返回true表示实际发生了wakeup所需的操作(此返回值不代表接口本身操作成功与否,因为此函数永远成功)
        bool                            wakeup();

        void                            runAsyncFunctor(UserFunctor&& f);
        void                            runFunctorAfterLoop(UserFunctor&& f);
        Timer::WeakPtr                  runAfter(nanoseconds timeout, UserFunctor&& callback);

        inline bool                     isInLoopThread() const
        {
            return mSelfThreadID == current_thread::tid();
        }

    private:
        void                            reallocEventSize(size_t size);
        void                            processAfterLoopFunctors();
        void                            processAsyncFunctors();
        void                            swapAsyncFunctors();
        void                            pushAsyncFunctor(UserFunctor&& f);

#ifndef PLATFORM_WINDOWS
        int                             getEpollHandle() const;
#endif
        bool                            linkChannel(sock fd, const Channel* ptr) BRYNET_NOEXCEPT;
        TcpConnectionPtr                getTcpConnection(sock fd);
        void                            addTcpConnection(sock fd, TcpConnectionPtr);
        void                            removeTcpConnection(sock fd);
        void                            tryInitThreadID();

    private:

#ifdef PLATFORM_WINDOWS
        std::vector<OVERLAPPED_ENTRY>   mEventEntries;

        typedef BOOL(WINAPI *sGetQueuedCompletionStatusEx) (HANDLE, LPOVERLAPPED_ENTRY, ULONG, PULONG, DWORD, BOOL);
        sGetQueuedCompletionStatusEx    mPGetQueuedCompletionStatusEx;
        HANDLE                          mIOCP;
#else
        std::vector<epoll_event>        mEventEntries;
        int                             mEpollFd;
#endif
        std::unique_ptr<WakeupChannel>  mWakeupChannel;

        std::atomic_bool                mIsInBlock;
        std::atomic_bool                mIsAlreadyPostWakeup;

        std::mutex                      mAsyncFunctorsMutex;
        std::vector<UserFunctor>        mAsyncFunctors;
        std::vector<UserFunctor>        mCopyAsyncFunctors;

        std::vector<UserFunctor>        mAfterLoopFunctors;
        std::vector<UserFunctor>        mCopyAfterLoopFunctors;

        std::once_flag                  mOnceInitThreadID;
        current_thread::THREAD_ID_TYPE  mSelfThreadID;

        timer::TimerMgr::Ptr            mTimer;
        std::unordered_map<sock, TcpConnectionPtr> mTcpConnections;

        friend class TcpConnection;
    };

} }