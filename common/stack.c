#include <string.h>
#include <stdlib.h>

#include "array.h"
#include "stack.h"

struct stack_s
{
    struct array_s* array;
    int element_num;

    int front;
    int tail;
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
            ret->tail = 0;
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
    self->tail = 0;
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
        self->tail = 0;
        free(self);
        self = NULL;
    }
}

bool
ox_stack_push(struct stack_s* self, const void* data)
{
    int top_index = self->tail;

    if(top_index >= self->element_num)
    {
        ox_stack_increase(self, ox_stack_size(self));
    }

    top_index = self->tail;
    if(top_index < self->element_num)
    {
        ox_array_set(self->array, top_index, data);
        self->tail++;
        return true;
    }

    return false;
}

static void
stack_check_init(struct stack_s* self)
{
    if(self->front == self->tail)
    {
        ox_stack_init(self);
    }
}

char*
ox_stack_front(struct stack_s* self)
{
    char* ret = NULL;

    if(self->front < self->tail)
    {
        ret = ox_array_at(self->array, self->front);
    }

    return ret;
}

char*
ox_stack_popfront(struct stack_s* self)
{
    char* ret = NULL;

    if(self->front < self->tail)
    {
        ret = ox_array_at(self->array, self->front);
        self->front++;
        stack_check_init(self);
    }

    return ret;
}

char*
ox_stack_popback(struct stack_s* self)
{
    char* ret = NULL;

    if(self->front < self->tail)
    {
        self->tail--;
        ret = ox_array_at(self->array, self->tail);
        stack_check_init(self);
    }
    
    return ret;
}

bool
ox_stack_isfull(struct stack_s* self)
{
    return (self->tail == self->element_num);
}

bool 
ox_stack_increase(struct stack_s* self, int increase_num)
{
    bool ret = ox_array_increase(self->array, increase_num);
    if(ret)
    {
        self->element_num = ox_array_num(self->array);
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
    return self->tail - self->front;
}

