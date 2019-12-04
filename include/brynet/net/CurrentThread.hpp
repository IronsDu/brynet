#pragma once

#include <brynet/base/Platform.hpp>

#ifdef BRYNET_PLATFORM_WINDOWS
#include <winsock2.h>
#include <Windows.h>
#elif defined BRYNET_PLATFORM_LINUX
#include <unistd.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <linux/unistd.h>
#elif defined BRYNET_PLATFORM_DARWIN
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#endif

namespace brynet { namespace net { namespace current_thread {

#ifdef BRYNET_PLATFORM_WINDOWS
    using THREAD_ID_TYPE = DWORD;
#elif defined BRYNET_PLATFORM_LINUX || defined BRYNET_PLATFORM_DARWIN
    using THREAD_ID_TYPE = int;
#endif

    static THREAD_ID_TYPE& tid()
    {
#ifdef BRYNET_PLATFORM_WINDOWS
        static __declspec(thread) THREAD_ID_TYPE cachedTid = 0;
#elif defined BRYNET_PLATFORM_LINUX || defined BRYNET_PLATFORM_DARWIN
        static __thread THREAD_ID_TYPE cachedTid = 0;
#endif

        if (cachedTid == 0)
        {
#ifdef BRYNET_PLATFORM_WINDOWS
            cachedTid = GetCurrentThreadId();
#elif defined BRYNET_PLATFORM_LINUX
            cachedTid = static_cast<pid_t>(::syscall(SYS_gettid));
#elif defined BRYNET_PLATFORM_DARWIN
            // warning: 'syscall' is deprecated: 
            // first deprecated in macOS 10.12 - syscall(2) is unsupported; 
            // please switch to a supported interface.
            uint64_t tid64;
            pthread_threadid_np(NULL, &tid64);
            cachedTid = (pid_t)tid64;
#endif
        }

        return cachedTid;
    }

} } }
