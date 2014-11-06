#ifndef _EVENT_LOOP_H
#define _EVENT_LOOP_H

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdint.h>

#include <functional>
#include <vector>
#include <mutex>
#include <thread>

using namespace std;

typedef BOOL(WINAPI *sGetQueuedCompletionStatusEx)
(HANDLE CompletionPort,
LPOVERLAPPED_ENTRY lpCompletionPortEntries,
ULONG ulCount,
PULONG ulNumEntriesRemoved,
DWORD dwMilliseconds,
BOOL fAlertable);

#define OVL_RECV    (1)
#define OVL_SEND    (2)

class Channel;

class EventLoop
{
    typedef std::function<void(void)>       USER_PROC;
    typedef std::function<void(Channel*)>   CONNECTION_ENTER_HANDLE;
public:
    EventLoop();

    void                            loop(int64_t    timeout);

    void                            wakeup();

    /*  投递一个链接，跟此eventloopEventLoop绑定，当被eventloop处理时会触发f回调  */
    void                            addConnection(int fd, Channel*, CONNECTION_ENTER_HANDLE f);

    /*  投递一个异步function，此eventloop被唤醒后，会回调此f*/
    void                            pushAsyncProc(USER_PROC f);

                                    /*  放入一个每次loop最后要执行的函数(TODO::仅在loop线程自身内调用)  */
                                    /*  用于实现一个datasocket有多个buffer要发送时，采用一个function进行合并包flush，则不是每一个buffer进行一次send   */
    void                            pushAfterLoopProc(USER_PROC f);

private:
    void                            recalocEventSize(int size);
    void                            linkConnection(int fd, Channel* ptr);

private:
    int                             mEventEntriesNum;
    OVERLAPPED_ENTRY*               mEventEntries;

    sGetQueuedCompletionStatusEx    mPGetQueuedCompletionStatusEx;
    HANDLE                          mIOCP;

    BOOL                            mInWaitIOEvent;
    BOOL                            mIsAlreadyPostedWakeUp;

    vector<USER_PROC>               mAsyncProcs;

    vector<USER_PROC>               mAfterLoopProcs;

    std::mutex                      mMutex;
    std::unique_lock<std::mutex>    mLock;
    std::thread::id                 mSelfThreadid;
};

#endif