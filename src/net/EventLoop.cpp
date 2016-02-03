#include <iostream>
#include <assert.h>
#include <thread>
#include <atomic>

#include "SocketLibFunction.h"
#include "Channel.h"
#include "EventLoop.h"

class WakeupChannel : public Channel
{
public:
    WakeupChannel(int fd)
    {
        mFd = fd;

    }
private:
    void    canRecv() override
    {
#ifdef PLATFORM_WINDOWS
#else
        /*  linux 下必须读完所有数据 */
        char temp[1024 * 10];
        while (true)
        {
            ssize_t n = read(mFd, temp, 1024 * 10);
            if (n == -1)
            {
                break;
            }
        }
#endif
    }

    void    canSend() override
    {
    }

    void    onClose() override
    {
    }

private:
    int     mFd;
};

EventLoop::EventLoop()
{
#ifdef PLATFORM_WINDOWS
    mPGetQueuedCompletionStatusEx = NULL;
    HMODULE kernel32_module = GetModuleHandleA("kernel32.dll");
    if (kernel32_module != NULL) {
        mPGetQueuedCompletionStatusEx = (sGetQueuedCompletionStatusEx)GetProcAddress(
            kernel32_module,
            "GetQueuedCompletionStatusEx");
    }
    mIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 1);
    memset(&mWakeupOvl, 0, sizeof(mWakeupOvl));
    mWakeupOvl.OP = EventLoop::OVL_RECV; 
    mWakeupChannel = new WakeupChannel(-1);
#else
    mEpollFd = epoll_create(1);
    mWakeupFd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    mWakeupChannel = new WakeupChannel(mWakeupFd);
    linkChannel(mWakeupFd, mWakeupChannel);
#endif
    mIsAlreadyPostWakeup = false;
    mIsInBlock = true;

    mEventEntries = NULL;
    mEventEntriesNum = 0;

    reallocEventSize(1024);
    mSelfThreadid = -1;
    mIsInitThreadID = false;
}

EventLoop::~EventLoop()
{
    delete mWakeupChannel;
    mWakeupChannel = nullptr;
#ifdef PLATFORM_WINDOWS
    CloseHandle(mIOCP);
    mIOCP = INVALID_HANDLE_VALUE;
#else
    close(mWakeupFd);
    mWakeupFd = -1;
    close(mEpollFd);
    mEpollFd = -1;
#endif
    delete[] mEventEntries;
    mEventEntries = nullptr;
}

TimerMgr& EventLoop::getTimerMgr()
{
    return mTimer;
}

void EventLoop::loop(int64_t timeout)
{
    if (!mIsInitThreadID)
    {
        mSelfThreadid = CurrentThread::tid();
        mIsInitThreadID = true;
    }

#ifndef NDEBUG
    assert(isInLoopThread());
#endif
    /*  warn::如果mAfterLoopProcs不为空（目前仅当第一次loop(时）之前就添加了回调或者某会话断开处理中又向其他会话发送消息而新产生callback），将timeout改为0，表示不阻塞iocp/epoll wait   */
    if (!mAfterLoopProcs.empty())
    {
        timeout = 0;
    }

#ifdef PLATFORM_WINDOWS

    ULONG numComplete = 0;
    if (mPGetQueuedCompletionStatusEx != nullptr)
    {
        BOOL rc = mPGetQueuedCompletionStatusEx(mIOCP, mEventEntries, mEventEntriesNum, &numComplete, static_cast<DWORD>(timeout), false);
        if (!rc)
        {
            numComplete = 0;
        }
    }
    else
    {
        /*对GQCS返回失败不予处理，正常流程下只存在于上层主动closesocket才发生此情况，且其会在onRecv中得到处理(最终释放会话资源)*/
        GetQueuedCompletionStatus(mIOCP,
                                            &mEventEntries[numComplete].dwNumberOfBytesTransferred,
                                            &mEventEntries[numComplete].lpCompletionKey,
                                            &mEventEntries[numComplete].lpOverlapped,
                                            static_cast<DWORD>(timeout));

        if (mEventEntries[numComplete].lpOverlapped != nullptr)
        {
            numComplete++;
            while (numComplete < mEventEntriesNum)
            {
                GetQueuedCompletionStatus(mIOCP,
                                               &mEventEntries[numComplete].dwNumberOfBytesTransferred,
                                               &mEventEntries[numComplete].lpCompletionKey,
                                               &mEventEntries[numComplete].lpOverlapped,
                                               0);
                if (mEventEntries[numComplete].lpOverlapped != nullptr)
                {
                    numComplete++;
                }
                else
                {
                    break;
                }
            }
        }
    }

    mIsInBlock = false;

    for (ULONG i = 0; i < numComplete; ++i)
    {
        Channel* ds = (Channel*)mEventEntries[i].lpCompletionKey;
        EventLoop::ovl_ext_s* ovl = (EventLoop::ovl_ext_s*)mEventEntries[i].lpOverlapped;
        if (ovl->OP == EventLoop::OVL_RECV)
        {
            ds->canRecv();
        }
        else if (ovl->OP == EventLoop::OVL_SEND)
        {
            ds->canSend();
        }
        else
        {
            assert(false);
        }
    }
#else
    int numComplete = epoll_wait(mEpollFd, mEventEntries, mEventEntriesNum, timeout);

    mIsInBlock = false;

    for (int i = 0; i < numComplete; ++i)
    {
        Channel*   ds = (Channel*)(mEventEntries[i].data.ptr);
        uint32_t event_data = mEventEntries[i].events;

        if (event_data & EPOLLRDHUP)
        {
            ds->canRecv();
            ds->onClose();   /*  无条件调用断开处理(安全的，不会造成重复close)，以防canRecv里没有recv 断开通知*/
        }
        else
        {
            if (event_data & EPOLLIN)
            {
                ds->canRecv();
            }

            if (event_data & EPOLLOUT)
            {
                ds->canSend();
            }
        }
    }
#endif

    mIsAlreadyPostWakeup = false;
    mIsInBlock = true;

    processAsyncProcs();
    processAfterLoopProcs();

    if (numComplete == mEventEntriesNum)
    {
        /*  如果事件被填充满了，则扩大事件结果队列大小，可以让一次epoll/iocp wait获得尽可能更多的通知 */
        reallocEventSize(mEventEntriesNum + 128);
    }

    mTimer.Schedule();
}

void EventLoop::processAfterLoopProcs()
{
    copyAfterLoopProcs.swap(mAfterLoopProcs);
    for (auto& x : copyAfterLoopProcs)
    {
        x();
    }
    copyAfterLoopProcs.clear();
}

void EventLoop::processAsyncProcs()
{
    std::vector<USER_PROC> temp;
    mAsyncProcsMutex.lock();
    temp.swap(mAsyncProcs);
    mAsyncProcsMutex.unlock();

    for (auto& x : temp)
    {
        x();
    }
}

bool EventLoop::isInLoopThread()
{
    return mSelfThreadid == CurrentThread::tid();
}

bool EventLoop::wakeup()
{
    bool ret = false;
    if (!isInLoopThread() && mIsInBlock && !mIsAlreadyPostWakeup.exchange(true))
    {
#ifdef PLATFORM_WINDOWS
        PostQueuedCompletionStatus(mIOCP, 0, (ULONG_PTR)mWakeupChannel, (OVERLAPPED*)&mWakeupOvl);
#else
        uint64_t one = 1;
        ssize_t n = write(mWakeupFd, &one, sizeof one);
#endif
        ret = true;
    }

    return ret;
}

bool EventLoop::linkChannel(int fd, Channel* ptr)
{
#ifdef PLATFORM_WINDOWS
    HANDLE ret = CreateIoCompletionPort((HANDLE)fd, mIOCP, (DWORD)ptr, 0);
    return ret != nullptr;
#else
    struct epoll_event ev = { 0, { 0 } };
    ev.events = EPOLLET | EPOLLIN | EPOLLRDHUP;
    ev.data.ptr = ptr;
    int ret = epoll_ctl(mEpollFd, EPOLL_CTL_ADD, fd, &ev);
    return ret == 0;
#endif
}

void EventLoop::pushAsyncProc(const USER_PROC& f)
{
    if (!isInLoopThread())
    {
        /*TODO::效率是否可以优化，多个线程同时添加异步函数，加锁导致效率下降*/
        mAsyncProcsMutex.lock();
        mAsyncProcs.push_back(f);
        mAsyncProcsMutex.unlock();

        wakeup();
    }
    else
    {
        f();
    }
}

void EventLoop::pushAsyncProc(USER_PROC&& f)
{
    if (!isInLoopThread())
    {
        /*TODO::效率是否可以优化，多个线程同时添加异步函数，加锁导致效率下降*/
        mAsyncProcsMutex.lock();
        mAsyncProcs.push_back(std::move(f));
        mAsyncProcsMutex.unlock();

        wakeup();
    }
    else
    {
        f();
    }
}

void EventLoop::pushAfterLoopProc(const USER_PROC& f)
{
    mAfterLoopProcs.push_back(f);
}

void EventLoop::pushAfterLoopProc(USER_PROC&& f)
{
    mAfterLoopProcs.push_back(std::move(f));
}

#ifdef PLATFORM_WINDOWS
HANDLE EventLoop::getIOCPHandle() const
{
    return mIOCP;
}
#else
int EventLoop::getEpollHandle() const
{
    return mEpollFd;
}
#endif

void EventLoop::reallocEventSize(int size)
{
    if (mEventEntries != NULL)
    {
        delete[] mEventEntries;
    }

#ifdef PLATFORM_WINDOWS
    mEventEntries = new OVERLAPPED_ENTRY[size];
#else
    mEventEntries = new epoll_event[size];
#endif

    mEventEntriesNum = size;
}