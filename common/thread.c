#include "platform.h"
#if defined PLATFORM_WINDOWS
#include <windows.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif

#include <stdlib.h>

#include "thread.h"

#if defined PLATFORM_WINDOWS
typedef DWORD thread_id;
#else
typedef pthread_t thread_id;
#endif

struct thread_s
{
    fpn_thread_fun   func;
    void*   param;
    thread_id id;
    bool    run;
#if defined PLATFORM_WINDOWS
    HANDLE  handle;
#else
#endif
};

#if defined PLATFORM_WINDOWS
DWORD static WINAPI fummy_run( void* param)
#else
void* fummy_run( void* param)
#endif
{
    struct thread_s* data = (struct thread_s*)param;
    data->func(data->param);
    return 0;
}

struct thread_s* 
ox_thread_new(fpn_thread_fun func, void* param)
{
    struct thread_s* data = (struct thread_s*)malloc(sizeof(struct thread_s));
    if(data != NULL)
    {
        data->func = func;
        data->param = param;
        data->run = true;
#if defined PLATFORM_WINDOWS
        data->handle = CreateThread( NULL, 0, fummy_run, data, 0, (PDWORD)&(data->id));
#else
        pthread_create( &(data->id), 0, fummy_run, data);
#endif
    }

    return data;
}

bool
ox_thread_isrun(struct thread_s* self)
{
    return self->run;
}

void 
ox_thread_delete(struct thread_s* self)
{
    self->run = false;
    ox_thread_wait(self);
    free(self);
    self = NULL;
}

void 
ox_thread_wait(struct thread_s* self)
{
    if(0 != self->id)
    {
#if defined PLATFORM_WINDOWS
        WaitForSingleObject(self->handle, INFINITE);
        CloseHandle(self->handle);
        self->handle = NULL;
#else
        pthread_join(self->id, NULL);
#endif

        self->id = 0;
    }
}

void 
ox_thread_sleep(int milliseconds)
{
#if defined PLATFORM_WINDOWS
    Sleep(milliseconds);
#else
    usleep(milliseconds * 1000 );
#endif
}
