#ifndef CURRENT_THREAD_H
#define CURRENT_THREAD_H

#include <thread>
#include "platform.h"

namespace CurrentThread
{
    typedef std::thread::id THREAD_ID_TYPE;
#ifdef PLATFORM_WINDOWS
    extern __declspec(thread) THREAD_ID_TYPE* cachedTid;
#else
    extern __thread THREAD_ID_TYPE* cachedTid;
#endif

    void cacheTid();

    inline THREAD_ID_TYPE& tid()
    {
        if (cachedTid == nullptr)
        {
            cacheTid();
        }

        return *cachedTid;
    }
}

#endif