#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "stack.h"
#include "mutex.h"
#include "rwlist.h"

struct rwlist_s
{
    struct mutex_s*    mutex;

    struct stack_s*    local_write;
    struct stack_s*    shared;
    struct stack_s*    local_read;

    struct thread_cond_s*   cond;

    int max_pengding_num;
};

struct rwlist_s* 
ox_rwlist_new(int num, int element_size, int max_pengding_num)
{
    struct rwlist_s* ret = (struct rwlist_s*)malloc(sizeof(struct rwlist_s));

    if(ret != NULL)
    {
        memset(ret, 0, sizeof(*ret));

        ret->local_write = ox_stack_new(num, element_size);
        ret->shared = ox_stack_new(num, element_size);
        ret->local_read = ox_stack_new(num, element_size);

        ret->mutex = ox_mutex_new();
        ret->cond = ox_thread_cond_new();

        ret->max_pengding_num = max_pengding_num;

        if(ret->local_write == NULL || ret->shared == NULL || ret->shared == NULL || ret->mutex == NULL)
        {
            ox_rwlist_delete(ret);
            ret = NULL;
        }
    }

    return ret;
}

void 
ox_rwlist_delete(struct rwlist_s* self)
{
    if(self != NULL)
    {
        if(self->mutex != NULL)
        {
            ox_mutex_lock(self->mutex);
        }
        
        if(self->local_write != NULL)
        {
            ox_stack_delete(self->local_write);
            self->local_write = NULL;
        }

        if(self->shared != NULL)
        {
            ox_stack_delete(self->shared);
            self->shared = NULL;
        }
        
        if(self->local_read != NULL)
        {
            ox_stack_delete(self->local_read);
            self->local_read = NULL;
        }

        if(self->cond != NULL)
        {
            ox_thread_cond_delete(self->cond);
            self->cond = NULL;
        }

        if(self->mutex != NULL)
        {
            ox_mutex_unlock(self->mutex);
        }

        if(self->mutex != NULL)
        {
            ox_mutex_delete(self->mutex);
            self->mutex = NULL;
        }

        free(self);
        self = NULL;
    }
}

static
void rwlist_sync_write(struct rwlist_s* self)
{
    if((ox_stack_num(self->shared) <= 0 && ox_stack_num(self->local_read) <= 0))
    {
        struct stack_s* temp;
        ox_mutex_lock(self->mutex);
        temp = self->shared;
        self->shared = self->local_write;
        self->local_write = temp;

        ox_thread_cond_signal(self->cond);

        ox_mutex_unlock(self->mutex);
    }
}

void
ox_rwlist_push(struct rwlist_s* self, const void* data)
{
    bool push_ret = ox_stack_push(self->local_write, data);
    assert(push_ret);
    /*  如果写队列数据大于最大值,且共享队列和读队列都没有数据则立即同步到共享队列  */
    if(ox_stack_num(self->local_write) > self->max_pengding_num && (ox_stack_num(self->shared) <= 0 && ox_stack_num(self->local_read) <= 0))
    {
        rwlist_sync_write(self);
    }
}

static
void rwlist_sync_read(struct rwlist_s* self, int timeout)
{
    struct stack_s* temp;

    /*  如果timeout为0且共享队列没有数据则函数直接返回,无须同步  */
    if(timeout <= 0 && ox_stack_num(self->shared) <= 0)
    {
        return;
    }

    ox_mutex_lock(self->mutex);

    if(ox_stack_num(self->shared) <= 0 && timeout > 0)
    {
        /*  如果共享队列没有数据且timeout大于0则需要等待通知,否则直接进行同步    */
        ox_thread_cond_timewait(self->cond, self->mutex, timeout);
    }

    if(ox_stack_num(self->shared) > 0)
    {
        temp = self->shared;
        self->shared = self->local_read;
        self->local_read = temp;
    }

    ox_mutex_unlock(self->mutex);
}

void
ox_rwlist_sync_read(struct rwlist_s* self, int timeout)
{
    if(ox_stack_num(self->local_read) <= 0)
    {
        /*  从共享队列同步到读队列    */
        rwlist_sync_read(self, timeout);
    }
}

char *
ox_rwlist_front(struct rwlist_s* self, int timeout)
{
    char * ret = NULL;
    if(ox_stack_num(self->local_read) <= 0)
    {
        /*  从共享队列同步到读队列    */
        rwlist_sync_read(self, timeout);
    }

    ret = ox_stack_front(self->local_read);

    return ret;
}

char *
ox_rwlist_pop(struct rwlist_s* self, int timeout)
{
    char * ret = NULL;
    if(ox_stack_num(self->local_read) <= 0)
    {
        /*  从共享队列同步到读队列    */
        rwlist_sync_read(self, timeout);
    }

    ret = ox_stack_popfront(self->local_read);

    return ret;
}

char *
ox_rwlist_once_front(struct rwlist_s* self)
{
    char * ret = ox_stack_front(self->local_read);

    return ret;
}

char *
ox_rwlist_once_pop(struct rwlist_s* self)
{
    char * ret = ox_stack_popfront(self->local_read);

    return ret;
}

void
ox_rwlist_flush(struct rwlist_s* self)
{
    if(ox_stack_num(self->local_write) > 0)
    {
        rwlist_sync_write(self);
    }
}

void
ox_rwlist_force_flush(struct rwlist_s* self)
{
    if(ox_stack_num(self->local_write) > 0)
    {
        if(ox_stack_num(self->shared) <= 0)
        {
            /*  如果共享队列没有数据则交换   */
            struct stack_s* temp;
            ox_mutex_lock(self->mutex);
            temp = self->shared;
            self->shared = self->local_write;
            self->local_write = temp;

            /*  thread_cond_signal(self->cond); */

            ox_mutex_unlock(self->mutex);
        }
        else
        {
            /*  否则将本地写队列里的数据放入到共享队列 */
            struct stack_s* temp;
            char* data = NULL;

            ox_mutex_lock(self->mutex);

            temp = self->local_write;
            while((data = ox_stack_popfront(temp)) != NULL)
            {
                ox_stack_push(self->shared, data);
            }

            ox_mutex_unlock(self->mutex);
        }
    }
}

bool
ox_rwlist_allempty(struct rwlist_s* self)
{
    return (ox_stack_num(self->local_write) + ox_stack_num(self->shared) + ox_stack_num(self->local_read)) <= 0;
}