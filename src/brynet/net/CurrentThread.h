#pragma once

#include <thread>
#include <brynet/net/Platform.h>

#ifdef PLATFORM_WINDOWS
#include <winsock2.h>
#include <Windows.h>
#else
#include <sys/types.h>
#endif

namespace brynet
{
    namespace net
    {
        namespace CurrentThread
        {
#ifdef PLATFORM_WINDOWS
            typedef DWORD THREAD_ID_TYPE;
            extern __declspec(thread) THREAD_ID_TYPE cachedTid;
#else
            typedef int THREAD_ID_TYPE;
            extern __thread THREAD_ID_TYPE cachedTid;
#endif

            THREAD_ID_TYPE& tid();
        }
    }
}
