#ifndef CURRENT_THREAD_H
#define CURRENT_THREAD_H

#include <thread>
#include "Platform.h"

#ifdef PLATFORM_WINDOWS
#include <Windows.h>
#else
#include <sys/syscall.h>
#include <sys/types.h>
#endif

namespace CurrentThread
{
#ifdef PLATFORM_WINDOWS
    typedef DWORD THREAD_ID_TYPE;
    extern __declspec(thread) THREAD_ID_TYPE cachedTid;
#else
    typedef int THREAD_ID_TYPE;
    extern __thread THREAD_ID_TYPE cachedTid;
#endif

    void cacheTid();

    inline THREAD_ID_TYPE& tid()
    {
        if (cachedTid == 0)
        {
            cacheTid();
        }

        return cachedTid;
    }
}

#endif
