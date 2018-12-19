#pragma once

#include <thread>
#include <brynet/net/Platform.h>

#ifdef PLATFORM_WINDOWS
#include <winsock2.h>
#include <Windows.h>
#else
#include <sys/types.h>
#endif

namespace brynet { namespace net { namespace current_thread {

#ifdef PLATFORM_WINDOWS
    using THREAD_ID_TYPE = DWORD;
    extern __declspec(thread) THREAD_ID_TYPE cachedTid;
#else
    using THREAD_ID_TYPE = int;
    extern __thread THREAD_ID_TYPE cachedTid;
#endif

    THREAD_ID_TYPE& tid();

} } }
