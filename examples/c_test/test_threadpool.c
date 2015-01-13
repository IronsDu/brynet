#include <stdio.h>

#include "platform.h"

#if defined PLATFORM_WINDOWS
#include <windows.h>
#else
#include <pthread.h>
#endif

#include "threadpool.h"
#include "mutex.h"
#include "thread.h"

static struct mutex_s* count_mutex = 0;
static int counter = 0;

static void showcounter()
{
    #if defined PLATFORM_WINDOWS
printf("############### ThreadID : %d\n", GetCurrentThreadId());
#else
printf("############### ThreadID : %d\n", (int)pthread_self());
#endif

    
    printf("wait counter lock\n");
	ox_mutex_lock(count_mutex);
    counter++;

    printf("current count : %d\n", counter);
	ox_mutex_unlock(count_mutex);

    printf("un counter lock\n");
}

static void my_msg_fun(struct thread_pool_s* self, void* msg)
{
    //thread_sleep(2000);
    showcounter();
}

struct thread_pool_s* p = NULL;

static void haha(void* arg)
{
    int i = 0;
    while(i < 3001)
    {
        thread_pool_pushmsg(p, (void*)3);
        i++;
    }
}

int main()
{
    int i= 0;
    struct thread_s* pthrea = NULL;
    p = thread_pool_new(my_msg_fun, 4, 4000);
	count_mutex = ox_mutex_new();
    thread_pool_start(p);
    
	ox_thread_sleep(1000);
    thread_pool_pushmsg(p, (void*)3);
    thread_pool_pushmsg(p, (void*)3);
    thread_pool_pushmsg(p, (void*)3);

    thread_pool_pushmsg(p, (void*)3);
    thread_pool_pushmsg(p, (void*)3);
    thread_pool_pushmsg(p, (void*)3);
    thread_pool_pushmsg(p, (void*)3);
    thread_pool_pushmsg(p, (void*)3);
    thread_pool_pushmsg(p, (void*)3);
    thread_pool_pushmsg(p, (void*)3);
    thread_pool_pushmsg(p, (void*)3);
    
    pthrea = ox_thread_new(haha, NULL);
    
    thread_pool_wait(p);

    printf("thread pool stop\n");

    thread_pool_wait(p);

    printf("thread pool stop\n");

    thread_pool_pushmsg(p, (void*)3);
    thread_pool_wait(p);

    printf("thread pool stop\n");
    //getchar();

    while(i < 3001)
    {
        thread_pool_pushmsg(p, (void*)3);
        i++;
    }
    
    ox_thread_wait(pthrea);
    thread_pool_delete(p);
    return 0;
}
