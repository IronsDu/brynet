#pragma once

#include <cstdbool>
#include <cstring>
#include <cstdlib>
#include <cassert>
#include <cstdint>

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
        if (self == nullptr)
        {
            return;
        }

        if (self->array != nullptr)
        {
            array_delete(self->array);
            self->array = nullptr;
        }

        self->element_num = 0;
        self->front = 0;
        self->num = 0;
        free(self);
        self = nullptr;
    }

    static struct stack_s* stack_new(size_t num, size_t element_size)
    {
        struct stack_s* ret = (struct stack_s*)malloc(sizeof(struct stack_s));
        if (ret == nullptr)
        {
            return nullptr;
        }

        ret->element_size = 0;
        ret->element_num = 0;
        ret->front = 0;
        ret->num = 0;
        ret->array = array_new(num, element_size);

        if (ret->array != nullptr)
        {
            ret->element_size = element_size;
            ret->element_num = num;
        }
        else
        {
            stack_delete(ret);
            ret = nullptr;
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
        if (tmp == nullptr)
        {
            return false;
        }

        {
            size_t current_num = self->element_num;
            size_t current_stack_num = stack_num(self);
            for (size_t i = 0; i < current_stack_num; ++i)
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
    
    /*  stack的stack_push会在空间不足的时候自动增长(通过stack_increase)  */
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

    static void* stack_front(struct stack_s* self)
    {
        void* ret = nullptr;

        if (stack_num(self) > 0)
        {
            ret = array_at(self->array, self->front);
        }

        return ret;
    }

    static void* stack_popfront(struct stack_s* self)
    {
        void* ret = stack_front(self);

        if (ret != nullptr)
        {
            self->num--;
            self->front++;
            self->front %= self->element_num;
        }

        return ret;
    }

    static void* stack_popback(struct stack_s* self)
    {
        void* ret = nullptr;

        if (stack_num(self) > 0)
        {
            self->num--;
            ret = array_at(self->array, (self->front + self->num) % self->element_num);
        }

        return ret;
    }

} }
