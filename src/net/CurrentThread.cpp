#include <thread>

#include "CurrentThread.h"

namespace CurrentThread
{
#ifdef PLATFORM_WINDOWS
    __declspec(thread) THREAD_ID_TYPE cachedTid = 0;
#else
    extern __thread THREAD_ID_TYPE cachedTid = 0;
#endif

    void cacheTid()
    {
#ifdef PLATFORM_WINDOWS
        cachedTid = GetCurrentThreadId();
#else
        cachedTid = static_cast<pid_t>(::syscall(SYS_gettid));
#endif
    }
}