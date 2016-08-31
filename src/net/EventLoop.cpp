#include <iostream>
#include <assert.h>
#include <thread>
#include <atomic>

#include "SocketLibFunction.h"
#include "Channel.h"
#include "EventLoop.h"

class WakeupChannel : public Channel, public NonCopyable
{
public:
    explicit WakeupChannel(sock fd) : mFd(fd)
    {
    }

private:
    ~WakeupChannel()
    {
#ifdef PLATFORM_WINDOWS
#else
        close(mFd);
        mFd = SOCKET_ERROR;
#endif
    }

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
    sock    mFd;
};

EventLoop::EventLoop()
#ifdef PLATFORM_WINDOWS
    : mWakeupOvl(EventLoop::OLV_VALUE::OVL_RECV)
#endif
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
    mTimer = std::make_shared<TimerMgr>();
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

TimerMgr::PTR EventLoop::getTimerMgr()
{
    tryInitThreadID();
    assert(isInLoopThread());
    return isInLoopThread() ? mTimer : nullptr;
}

inline void EventLoop::tryInitThreadID()
{
    if (!mIsInitThreadID)
    {
        mSelfThreadid = CurrentThread::tid();
        mIsInitThreadID = true;
    }
}

void EventLoop::loop(int64_t timeout)
{
    tryInitThreadID();

#ifndef NDEBUG
    assert(isInLoopThread());
#endif
    if (!isInLoopThread())
    {
        return;
    }

    /*  warn::如果mAfterLoopProcs不为空（目前仅当第一次loop(时）之前就添加了回调或者某会话断开处理中又向其他会话发送消息而新产生callback），将timeout改为0，表示不阻塞iocp/epoll wait   */
    if (!mAfterLoopProcs.empty())
    {
        timeout = 0;
    }

#ifdef PLATFORM_WINDOWS

    ULONG numComplete = 0;
    if (mPGetQueuedCompletionStatusEx != nullptr)
    {
        BOOL rc = mPGetQueuedCompletionStatusEx(mIOCP, mEventEntries, static_cast<ULONG>(mEventEntriesNum), &numComplete, static_cast<DWORD>(timeout), false);
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
        Channel* channel = (Channel*)mEventEntries[i].lpCompletionKey;
        EventLoop::ovl_ext_s* ovl = (EventLoop::ovl_ext_s*)mEventEntries[i].lpOverlapped;
        if (ovl->OP == EventLoop::OLV_VALUE::OVL_RECV)
        {
            channel->canRecv();
        }
        else if (ovl->OP == EventLoop::OLV_VALUE::OVL_SEND)
        {
            channel->canSend();
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
        Channel*    channel = (Channel*)(mEventEntries[i].data.ptr);
        uint32_t    event_data = mEventEntries[i].events;

        if (event_data & EPOLLRDHUP)
        {
            channel->canRecv();
            channel->onClose();   /*  无条件调用断开处理(安全的，不会造成重复close)，以防canRecv里没有recv 断开通知*/
        }
        else
        {
            if (event_data & EPOLLIN)
            {
                channel->canRecv();
            }

            if (event_data & EPOLLOUT)
            {
                channel->canSend();
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

    mTimer->Schedule();
}

void EventLoop::processAfterLoopProcs()
{
    mCopyAfterLoopProcs.swap(mAfterLoopProcs);
    for (auto& x : mCopyAfterLoopProcs)
    {
        x();
    }
    mCopyAfterLoopProcs.clear();
}

void EventLoop::processAsyncProcs()
{
    mAsyncProcsMutex.lock();
    mCopyAsyncProcs.swap(mAsyncProcs);
    mAsyncProcsMutex.unlock();

    for (auto& x : mCopyAsyncProcs)
    {
        x();
    }
    mCopyAsyncProcs.clear();
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

bool EventLoop::linkChannel(sock fd, Channel* ptr)
{
#ifdef PLATFORM_WINDOWS
    return CreateIoCompletionPort((HANDLE)fd, mIOCP, (ULONG_PTR)ptr, 0) != nullptr;
#else
    struct epoll_event ev = { 0, { 0 } };
    ev.events = EPOLLET | EPOLLIN | EPOLLRDHUP;
    ev.data.ptr = ptr;
    return epoll_ctl(mEpollFd, EPOLL_CTL_ADD, fd, &ev) == 0;
#endif
}

void EventLoop::pushAsyncProc(const USER_PROC& f)
{
    if (!isInLoopThread())
    {
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
    assert(isInLoopThread());
    if (isInLoopThread())
    {
        mAfterLoopProcs.push_back(f);
    }
}

void EventLoop::pushAfterLoopProc(USER_PROC&& f)
{
    assert(isInLoopThread());
    if (isInLoopThread())
    {
        mAfterLoopProcs.push_back(std::move(f));
    }
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

void EventLoop::reallocEventSize(size_t size)
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