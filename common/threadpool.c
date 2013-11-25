#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

#include "mutex.h"
#include "stack.h"
#include "thread.h"

#include "threadpool.h"

struct thread_pool_s
{
    bool is_run;

    struct stack_s* msg_list;

    struct mutex_s* msg_lock;

    struct thread_s** work_threads;
    bool* work_thread_state;

    struct thread_cond_s* cv;

    thread_msg_fun_pt callback;
    int work_thread_num;

    int thread_index;
    struct mutex_s* thread_index_lock;
};

struct thread_pool_s* 
thread_pool_new(thread_msg_fun_pt callback, int thread_num, int msg_num)
{
    struct thread_pool_s* ret = (struct thread_pool_s*)malloc(sizeof(*ret));
    if(ret != NULL)
    {
        ret->msg_list = ox_stack_new(msg_num,sizeof(void*));
        ret->msg_lock = ox_mutex_new();

        ret->cv = ox_thread_cond_new();

        ret->callback = callback;
        ret->work_thread_num = thread_num;
        ret->work_threads = (struct thread_s**)malloc(sizeof(struct thread_s*)*ret->work_thread_num);
        memset(ret->work_threads, 0, sizeof(struct thread_s*)*ret->work_thread_num);

        ret->work_thread_state = (bool*)malloc(sizeof(bool)*ret->work_thread_num);
        memset(ret->work_thread_state, false, sizeof(bool)*ret->work_thread_num);

        ret->thread_index_lock = ox_mutex_new();
        ret->thread_index = 0;

        ret->is_run = true;
    }

    return ret;
}

void 
thread_pool_delete(struct thread_pool_s* self)
{
    thread_pool_stop(self);

    ox_stack_delete(self->msg_list);
    self->msg_list = NULL;

    ox_mutex_delete(self->msg_lock);
    self->msg_lock = NULL;

    free(self->work_threads);

    ox_thread_cond_delete(self->cv);
    self->cv = 0;

    ox_mutex_delete(self->thread_index_lock);
    self->thread_index_lock = NULL;

    free(self);
    self = NULL;
}

static void 
thread_pool_work(void* arg)
{
    struct thread_pool_s* self = (struct thread_pool_s*)arg;
    struct stack_s* msg_list = self->msg_list;
    int thread_index = 0;
    bool* thread_state = self->work_thread_state;
    
    thread_msg_fun_pt callback = self->callback;
    void** msg_ptr = NULL;
    
    ox_mutex_lock(self->thread_index_lock);
    thread_index = self->thread_index++;
    ox_mutex_unlock(self->thread_index_lock);

    for(;;)
    {
        ox_mutex_lock(self->msg_lock);
        
        while(self->is_run && ox_stack_num(msg_list) <= 0)
        {
            ox_thread_cond_wait(self->cv, self->msg_lock);
        }
        
        if(!self->is_run)
        {
            ox_mutex_unlock(self->msg_lock);
            ox_thread_cond_signal(self->cv);
            break;
        }
        
        msg_ptr = (void**)ox_stack_popback(self->msg_list);
        
        ox_mutex_unlock(self->msg_lock);
        
        thread_state[thread_index] = true;

        if(msg_ptr != NULL)
        {
            (*callback)(self, *msg_ptr);
        }

        thread_state[thread_index] = false;
    }
}

void 
thread_pool_start(struct thread_pool_s* self)
{
    if(self != NULL)
    {
        int i = 0;

        for(; i < self->work_thread_num; ++i)
        {
            if(self->work_threads[i] == NULL)
            {
                self->work_threads[i] = ox_thread_new(thread_pool_work, self);
            }
        }
    }
}

void 
thread_pool_stop(struct thread_pool_s* self)
{
    if(self != NULL)
    {
        int i = 0;
        self->is_run = false;

        for(; i < self->work_thread_num; ++i)
        {
            if(NULL != self->work_threads[i])
            {
                ox_thread_cond_signal(self->cv);
                ox_thread_delete(self->work_threads[i]);
            }
        }

        memset(self->work_threads, 0, sizeof(struct thread_s*)*self->work_thread_num);
    }
}

/*  检测线程池是否忙(没有处理完所有消息) */
static bool 
thread_pool_isbusy(struct thread_pool_s* self)
{
    bool isbusy = false;

    if(ox_stack_num(self->msg_list) > 0)
    {
        isbusy = true;
    }
    else
    {
        int thread_num = self->work_thread_num;
        int i = 0;

        for(; i < thread_num; ++i)
        {
            if(self->work_thread_state[i])
            {
                isbusy = true;
                break;
            }
        }
    }

    return isbusy;
}

void 
thread_pool_wait(struct thread_pool_s* self)
{
    /*  循环等待线程池,直到其处于非忙状态   */
    for(;;)
    {
        if(!thread_pool_isbusy(self))
        {
            break;
        }

        ox_thread_sleep(1);
    }
}

bool 
thread_pool_pushmsg(struct thread_pool_s* self, void* data)
{
    /*  添加消息,并触发条件变量    */
    bool push_ret = false;
    
    if(self->is_run)
    {
        ox_mutex_lock(self->msg_lock);

        if(ox_stack_push(self->msg_list, &data))
        {
            push_ret = true;
        }
        ox_mutex_unlock(self->msg_lock);

        ox_thread_cond_signal(self->cv);
    }
    
    return push_ret;
}
