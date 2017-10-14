#ifndef BRYNET_NET_EVENTLOOP_H_
#define BRYNET_NET_EVENTLOOP_H_

#include <cstdint>
#include <functional>
#include <vector>
#include <mutex>
#include <memory>
#include <atomic>

#include <brynet/net/CurrentThread.h>
#include <brynet/net/SocketLibFunction.h>
#include <brynet/timer/Timer.h>
#include <brynet/utils/NonCopyable.h>

namespace brynet
{
    namespace net
    {
        class Channel;
        class DataSocket;
        class WakeupChannel;

        class EventLoop : public NonCopyable
        {
        public:
            typedef std::shared_ptr<EventLoop>          PTR;
            typedef std::function<void(void)>           USER_PROC;

#ifdef PLATFORM_WINDOWS
            enum class OLV_VALUE
            {
                OVL_NONE = 0,
                OVL_RECV,
                OVL_SEND,
            };

            struct ovl_ext_s
            {
                OVERLAPPED  base;
                const EventLoop::OLV_VALUE  OP;

                ovl_ext_s(OLV_VALUE op) : OP(op)
                {
                    memset(&base, 0, sizeof(base));
                }
            };
#endif

        public:
            EventLoop() noexcept;
            virtual ~EventLoop() noexcept;

            void                            loop(int64_t milliseconds);
            bool                            wakeup();

            void                            pushAsyncProc(USER_PROC f);
            void                            pushAfterLoopProc(USER_PROC f);

            /* return nullptr if not called in net thread*/
            TimerMgr::PTR                   getTimerMgr();

            inline bool                     isInLoopThread() const
            {
                return mSelfThreadID == CurrentThread::tid();
            }

        private:
            void                            reallocEventSize(size_t size);
            void                            processAfterLoopProcs();
            void                            processAsyncProcs();

#ifndef PLATFORM_WINDOWS
            int                             getEpollHandle() const;
#endif
            bool                            linkChannel(sock fd, Channel* ptr);
            void                            tryInitThreadID();

        private:

#ifdef PLATFORM_WINDOWS
            OVERLAPPED_ENTRY*               mEventEntries;

            typedef BOOL(WINAPI *sGetQueuedCompletionStatusEx) (HANDLE, LPOVERLAPPED_ENTRY, ULONG, PULONG, DWORD, BOOL);
            sGetQueuedCompletionStatusEx    mPGetQueuedCompletionStatusEx;
            HANDLE                          mIOCP;
#else
            epoll_event*                    mEventEntries;
            int                             mEpollFd;
#endif
            size_t                          mEventEntriesNum;
            std::unique_ptr<WakeupChannel>  mWakeupChannel;

            std::atomic_bool                mIsInBlock;
            std::atomic_bool                mIsAlreadyPostWakeup;

            std::mutex                      mAsyncProcsMutex;
            std::vector<USER_PROC>          mAsyncProcs;                
            std::vector<USER_PROC>          mCopyAsyncProcs;

            std::vector<USER_PROC>          mAfterLoopProcs;
            std::vector<USER_PROC>          mCopyAfterLoopProcs;

            std::once_flag                  mOnceInitThreadID;
            CurrentThread::THREAD_ID_TYPE   mSelfThreadID;             

            TimerMgr::PTR                   mTimer;

            friend class DataSocket;
        };
    }
}

#endif