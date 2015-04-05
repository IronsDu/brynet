#include <thread>

#include "currentthread.h"

namespace CurrentThread
{
#ifdef PLATFORM_WINDOWS
    __declspec(thread) THREAD_ID_TYPE* cachedTid = nullptr;
#else
    extern __thread THREAD_ID_TYPE* cachedTid = nullptr;
#endif

    void cacheTid()
    {
        cachedTid = new std::thread::id();
        *cachedTid = std::this_thread::get_id();
    }
}