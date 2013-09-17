#include <stdlib.h>
#include <stdint.h>
#include "platform.h"

#if defined PLATFORM_WINDOWS
#include <windows.h>
#else
#include <pthread.h>
#include <sys/time.h>
#endif

#include "mutex.h"

struct mutex_s
{
#if defined PLATFORM_WINDOWS
    CRITICAL_SECTION    mutex;
#else
    pthread_mutex_t    mutex;
#endif
};

struct thread_cond_s
{
#if defined PLATFORM_WINDOWS
    HANDLE  cond;
#else
    pthread_cond_t  cond;
#endif

};

#ifndef PLATFORM_WINDOWS
struct thread_spinlock_s
{
    pthread_spinlock_t spinlock;
};
#endif

struct mutex_s* 
ox_mutex_new(void)
{
    struct mutex_s* ret = (struct mutex_s*)malloc(sizeof(struct mutex_s));
    if(ret != NULL)
    {
        #if defined PLATFORM_WINDOWS
        InitializeCriticalSection(&ret->mutex);
        #else
        pthread_mutex_init(&ret->mutex, 0);
        #endif
    }

    return ret;
}

void 
ox_mutex_delete(struct mutex_s* self)
{
    if(self != NULL)
    {
#if defined PLATFORM_WINDOWS
        DeleteCriticalSection(&self->mutex);
#else
        pthread_mutex_destroy(&self->mutex);
#endif

        free(self);
        self = NULL;
    }
}

void 
ox_mutex_lock(struct mutex_s* self)
{
    #if defined PLATFORM_WINDOWS
    EnterCriticalSection(&self->mutex);
    #else
    pthread_mutex_lock(&self->mutex);
    #endif
}

void 
ox_mutex_unlock(struct mutex_s* self)
{
    #if defined PLATFORM_WINDOWS
    LeaveCriticalSection(&self->mutex);
    #else
    pthread_mutex_unlock(&self->mutex);
    #endif
}

struct thread_cond_s* 
ox_thread_cond_new(void)
{
    struct thread_cond_s* ret = (struct thread_cond_s*)malloc(sizeof(*ret));
    if(ret != NULL)
    {
#if defined PLATFORM_WINDOWS
        ret->cond = CreateEvent(NULL, FALSE, FALSE, NULL );
#else
        pthread_cond_init(&ret->cond, 0);
#endif
    }

    return ret;
}

void 
ox_thread_cond_delete(struct thread_cond_s* self)
{
    if(self != NULL)
    {
#if defined PLATFORM_WINDOWS
        CloseHandle(self->cond);
#else
        pthread_cond_destroy(&self->cond);
#endif

        free(self);
        self = NULL;
    }
}

void 
ox_thread_cond_wait(struct thread_cond_s* self, struct mutex_s* mutex)
{
#if defined PLATFORM_WINDOWS
    ox_mutex_unlock(mutex);
    WaitForSingleObject(self->cond, INFINITE);
    ox_mutex_lock(mutex);
#else
    pthread_cond_wait(&self->cond, &mutex->mutex);
#endif
}

void
ox_thread_cond_timewait(struct thread_cond_s* self, struct mutex_s* mutex, unsigned int timeout)
{
#if defined PLATFORM_WINDOWS
    ox_mutex_unlock(mutex);
    WaitForSingleObject(self->cond, timeout);
    ox_mutex_lock(mutex);
#else
    struct timespec ts;
    struct timeval now;
    uint64_t sec = 0;
    uint64_t msec = 0;
    gettimeofday(&now, NULL);
    sec = timeout/1000;
    msec = timeout%1000;
    ts.tv_sec = now.tv_sec + sec;
    ts.tv_nsec = now.tv_usec * 1000 + msec*1000*1000;

    ts.tv_sec += (ts.tv_nsec / 1000000000);
    ts.tv_nsec %= 1000000000;

    pthread_cond_timedwait(&self->cond, &mutex->mutex, &ts);
#endif
}

void 
ox_thread_cond_signal(struct thread_cond_s* self)
{
#if defined PLATFORM_WINDOWS
    SetEvent(self->cond);
#else
    pthread_cond_signal(&self->cond);
#endif
}

#ifndef PLATFORM_WINDOWS
struct thread_spinlock_s* 
ox_thread_spinlock_new(void)
{
    struct thread_spinlock_s* ret = (struct thread_spinlock_s*)malloc(sizeof(*ret));
    if(ret != NULL)
    {
        pthread_spin_init(&ret->spinlock, 0);
    }

    return ret;    
}

void 
ox_thread_spinlock_delete(struct thread_spinlock_s* self)
{
    pthread_spin_destroy(&self->spinlock);
    free(self);
}

void 
ox_thread_spinlock_lock(struct thread_spinlock_s* self)
{
    pthread_spin_lock(&self->spinlock);
}

void 
ox_thread_spinlock_unlock(struct thread_spinlock_s* self)
{
    pthread_spin_unlock(&self->spinlock);
}

#endif
