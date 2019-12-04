#pragma once

#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>

#include <brynet/base/Array.hpp>

namespace brynet { namespace base {

    struct stack_s
    {
        struct array_s* array;
        size_t          element_size;

        size_t          element_num;
        size_t          front;              /*  栈底  */
        size_t          num;                /*  栈有效元素大小 */
    };

    static void stack_delete(struct stack_s* self)
    {
        if (self == NULL)
        {
            return;
        }

        if (self->array != NULL)
        {
            array_delete(self->array);
            self->array = NULL;
        }

        self->element_num = 0;
        self->front = 0;
        self->num = 0;
        free(self);
        self = NULL;
    }

    static struct stack_s* stack_new(size_t num, size_t element_size)
    {
        struct stack_s* ret = (struct stack_s*)malloc(sizeof(struct stack_s));
        if (ret == NULL)
        {
            return NULL;
        }

        memset(ret, 0, sizeof(*ret));

        ret->element_size = element_size;
        ret->array = array_new(num, element_size);

        if (ret->array != NULL)
        {
            ret->element_num = num;
            ret->num = 0;
        }
        else
        {
            stack_delete(ret);
            ret = NULL;
        }

        return ret;
    }

    static void stack_init(struct stack_s* self)
    {
        self->front = 0;
        self->num = 0;
    }

    static size_t stack_num(struct stack_s* self)
    {
        return self->num;
    }

    static bool stack_increase(struct stack_s* self, size_t increase_num)
    {
        struct array_s* tmp = array_new(self->element_num + increase_num, 
            self->element_size);
        if (tmp == NULL)
        {
            return false;
        }

        {
            size_t i = 0;
            size_t current_num = self->element_num;
            size_t current_stack_num = stack_num(self);
            for (; i < current_stack_num; ++i)
            {
                array_set(tmp, i, array_at(self->array, (self->front + i) % current_num));
            }

            self->front = 0;
            array_delete(self->array);
            self->array = tmp;
            self->element_num = array_num(self->array);
        }

        return true;
    }

    static size_t stack_size(struct stack_s* self)
    {
        return self->element_num;
    }

    static bool stack_isfull(struct stack_s* self)
    {
        return (self->num == self->element_num);
    }

    static bool stack_push(struct stack_s* self, const void* data)
    {
        if (stack_isfull(self))
        {
            stack_increase(self, stack_size(self));
        }

        if (stack_isfull(self))
        {
            return false;
        }

        array_set(self->array, (self->front + self->num) % self->element_num, data);
        self->num++;
        return true;
    }

    static char* stack_front(struct stack_s* self)
    {
        char* ret = NULL;

        if (stack_num(self) > 0)
        {
            ret = array_at(self->array, self->front);
        }

        return ret;
    }

    static char* stack_popfront(struct stack_s* self)
    {
        char* ret = stack_front(self);

        if (ret != NULL)
        {
            self->num--;
            self->front++;
            self->front %= self->element_num;
        }

        return ret;
    }

    static char* stack_popback(struct stack_s* self)
    {
        char* ret = NULL;

        if (stack_num(self) > 0)
        {
            self->num--;
            ret = array_at(self->array, (self->front + self->num) % self->element_num);
        }

        return ret;
    }

} }
