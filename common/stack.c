#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "array.h"
#include "stack.h"

struct stack_s
{
    struct array_s* array;
    int element_num;        /*  当前array空间大小 */

    int front;              /*  栈底  */
    int num;                /*  栈有效元素大小 */
};

struct stack_s* 
ox_stack_new(int num, int element_size)
{
    struct stack_s* ret = (struct stack_s*)malloc(sizeof(struct stack_s));

    if(ret != NULL)
    {
        memset(ret, 0, sizeof(*ret));

        ret->array = ox_array_new(num, element_size);

        if(ret->array != NULL)
        {
            ret->element_num = num;
            ret->num = 0;
        }
        else
        {
            ox_stack_delete(ret);
            ret = NULL;
        }
    }

    return ret;
}

void
ox_stack_init(struct stack_s* self)
{
    self->front = 0;
    self->num = 0;
}

void 
ox_stack_delete(struct stack_s* self)
{
    if(self != NULL)
    {
        if(self->array != NULL)
        {
            ox_array_delete(self->array);
            self->array = NULL;
        }
        
        self->element_num = 0;
        self->front = 0;
        self->num = 0;
        free(self);
        self = NULL;
    }
}

bool
ox_stack_push(struct stack_s* self, const void* data)
{
    if(ox_stack_isfull(self))
    {
        ox_stack_increase(self, ox_stack_size(self));
    }

    if(!ox_stack_isfull(self))
    {
        ox_array_set(self->array, (self->front + self->num) % self->element_num, data);
        self->num++;
        return true;
    }

    return false;
}

char*
ox_stack_front(struct stack_s* self)
{
    char* ret = NULL;

    if(ox_stack_num(self) > 0)
    {
        ret = ox_array_at(self->array, self->front);
    }

    return ret;
}

char*
ox_stack_popfront(struct stack_s* self)
{
    char* ret = ox_stack_front(self);

    if(ret != NULL)
    {
        self->num--;
        self->front++;
        self->front %= self->element_num;
    }

    return ret;
}

char*
ox_stack_popback(struct stack_s* self)
{
    char* ret = NULL;

    if(ox_stack_num(self) > 0)
    {
        self->num--;
        ret = ox_array_at(self->array, (self->front + self->num) % self->element_num);
    }
    
    return ret;
}

bool
ox_stack_isfull(struct stack_s* self)
{
    return (self->num == self->element_num);
}

bool 
ox_stack_increase(struct stack_s* self, int increase_num)
{
    bool ret = false;
    int i = 0;
    for(; i < ox_stack_num(self); ++i)
    {
        ox_array_set(self->array, i, ox_array_at(self->array, self->front+i));
    }

    self->front = 0;

    ret = ox_array_increase(self->array, increase_num);
    if(ret)
    {
        self->element_num = ox_array_num(self->array);
    }
    else
    {
        assert(false);
    }
    return ret;
}

int 
ox_stack_size(struct stack_s* self)
{
    return self->element_num;
}

int 
ox_stack_num(struct stack_s* self)
{
    return self->num;
}

