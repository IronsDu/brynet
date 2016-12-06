#ifndef DODO_NET_CURRENTTHREAD_H_
#define DODO_NET_CURRENTTHREAD_H_

#include <thread>
#include "Platform.h"

#ifdef PLATFORM_WINDOWS
#include <winsock2.h>
#include <Windows.h>
#else
#include <sys/syscall.h>
#include <sys/types.h>
#endif

namespace dodo
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
    }
}

#endif
