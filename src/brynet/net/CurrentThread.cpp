#include <brynet/net/CurrentThread.h>

#ifdef PLATFORM_WINDOWS
#else
#include <unistd.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <linux/unistd.h>
#endif

namespace brynet
{
    namespace net
    {
        namespace CurrentThread
        {
#ifdef PLATFORM_WINDOWS
__declspec(thread) THREAD_ID_TYPE cachedTid = 0;
#else
__thread THREAD_ID_TYPE cachedTid = 0;
#endif
        }
    }
}

brynet::net::CurrentThread::THREAD_ID_TYPE& brynet::net::CurrentThread::tid()
{
    if (cachedTid == 0)
    {
#ifdef PLATFORM_WINDOWS
        cachedTid = GetCurrentThreadId();
#else
        cachedTid = static_cast<pid_t>(::syscall(SYS_gettid));
#endif
    }

    return cachedTid;
}
