#include <brynet/net/CurrentThread.h>

#ifdef PLATFORM_LINUX
#include <unistd.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <linux/unistd.h>
#elif defined PLATFORM_DARWIN
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#endif

namespace brynet { namespace net { namespace current_thread {

#ifdef PLATFORM_WINDOWS
    __declspec(thread) THREAD_ID_TYPE cachedTid = 0;
#elif defined PLATFORM_LINUX || defined PLATFORM_DARWIN
    __thread THREAD_ID_TYPE cachedTid = 0;
#endif
    THREAD_ID_TYPE& tid()
    {
        if (cachedTid == 0)
        {
#ifdef PLATFORM_WINDOWS
            cachedTid = GetCurrentThreadId();
#elif defined PLATFORM_LINUX
            cachedTid = static_cast<pid_t>(::syscall(SYS_gettid));
#elif defined PLATFORM_DARWIN
            //warning: 'syscall' is deprecated: first deprecated in macOS 10.12 - syscall(2) is unsupported; please switch to a supported interface.
            uint64_t tid64;
            pthread_threadid_np(NULL, &tid64);
            cachedTid = (pid_t)tid64;
#endif
        }

        return cachedTid;
    }

} } }