#ifndef _EVENT_LOOP_H
#define _EVENT_LOOP_H

#include <stdint.h>
#include <functional>
#include <vector>
#include <mutex>
#include <thread>
#include <memory>
#include <unordered_map>

#include "CurrentThread.h"
#include "SocketLibFunction.h"
#include "timer.h"

class Channel;

class EventLoop
{
public:
    typedef std::function<void(void)>           USER_PROC;

#ifdef PLATFORM_WINDOWS
    enum OLV_VALUE
    {
        OVL_RECV = 1,
        OVL_SEND,
        OVL_CLOSE,
    };

    struct ovl_ext_s
    {
        OVERLAPPED  base;
        int         OP;
    };
#endif

public:
    EventLoop();
    ~EventLoop();

    void                            loop(int64_t    timeout);

    bool                            wakeup();

    /*  投递一个异步回调，在EventLoop::loop被唤醒后执行 */
    void                            pushAsyncProc(const USER_PROC& f);
    void                            pushAsyncProc(USER_PROC&& f);

    /*  非线程安全:投递回调放置在单次loop结尾时执行   */
    void                            pushAfterLoopProc(const USER_PROC& f);
    void                            pushAfterLoopProc(USER_PROC&& f);

    TimerMgr&                       getTimerMgr();

    bool                            linkChannel(int fd, Channel* ptr);

#ifdef PLATFORM_WINDOWS
    HANDLE                          getIOCPHandle() const;
#else
    int                             getEpollHandle() const;
#endif

private:
    void                            reallocEventSize(int size);
    void                            processAfterLoopProcs();
    void                            processAsyncProcs();

    bool                            isInLoopThread();

private:
    size_t                          mEventEntriesNum;
#ifdef PLATFORM_WINDOWS
    typedef BOOL(WINAPI *sGetQueuedCompletionStatusEx) (HANDLE ,LPOVERLAPPED_ENTRY ,ULONG ,PULONG ,DWORD ,BOOL );

    OVERLAPPED_ENTRY*               mEventEntries;
    sGetQueuedCompletionStatusEx    mPGetQueuedCompletionStatusEx;
    HANDLE                          mIOCP;
    ovl_ext_s                       mWakeupOvl;
    Channel*                        mWakeupChannel;
#else
    int                             mEpollFd;
    epoll_event*                    mEventEntries;
    int                             mWakeupFd;
    Channel*                        mWakeupChannel;
#endif

    std::mutex                      mFlagMutex;

    bool                            mInWaitIOEvent;             /*  如果为false表示肯定没有处于epoll/iocp wait，如果为true，表示即将或已经等待*/
    bool                            mIsAlreadyPostedWakeUp;     /*  表示是否已经投递过wakeup(避免其他线程投递太多(不必要)的wakeup) */

    std::vector<USER_PROC>          mAsyncProcs;                /*  投递到此eventloop的异步function队列    */

    std::vector<USER_PROC>          mAfterLoopProcs;            /*  eventloop每次循环的末尾要执行的一系列函数   */
    std::vector<USER_PROC>          copyAfterLoopProcs;         /*  用于在loop中代替mAfterLoopProcs进行遍历，避免遍历途中又添加新元素  */

    std::mutex                      mAsyncProcsMutex;

    bool                            mIsInitThreadID;
    CurrentThread::THREAD_ID_TYPE   mSelfThreadid;              /*  调用loop函数所在thread的id */

    TimerMgr                        mTimer;
};

#endif