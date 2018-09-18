#ifndef BRYNET_NET_EVENTLOOP_H_
#define BRYNET_NET_EVENTLOOP_H_

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

namespace brynet
{
    namespace net
    {
        class Channel;
        class DataSocket;
        typedef std::shared_ptr<DataSocket> DataSocketPtr;
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

                ovl_ext_s(OLV_VALUE op) BRYNET_NOEXCEPT : OP(op)
                {
                    memset(&base, 0, sizeof(base));
                }
            };
#endif

        public:
            EventLoop() BRYNET_NOEXCEPT;
            virtual ~EventLoop() BRYNET_NOEXCEPT;

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
            bool                            linkChannel(sock fd, const Channel* ptr) BRYNET_NOEXCEPT;
            DataSocketPtr                   getDataSocket(sock fd);
            void                            addDataSocket(sock fd, DataSocketPtr);
            void                            removeDataSocket(sock fd);
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

            std::mutex                      mAsyncProcsMutex;
            std::vector<USER_PROC>          mAsyncProcs;                
            std::vector<USER_PROC>          mCopyAsyncProcs;

            std::vector<USER_PROC>          mAfterLoopProcs;
            std::vector<USER_PROC>          mCopyAfterLoopProcs;

            std::once_flag                  mOnceInitThreadID;
            CurrentThread::THREAD_ID_TYPE   mSelfThreadID;             

            TimerMgr::PTR                   mTimer;
            std::unordered_map<sock, DataSocketPtr> mDataSockets;

            friend class DataSocket;
        };
    }
}

#endif