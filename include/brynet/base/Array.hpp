#pragma once

#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

namespace brynet { namespace base {

    struct array_s
    {
        char*   buffer;
        size_t  buffer_size;
        size_t  element_size;
        size_t  element_num;
    };

    static void array_delete(struct array_s* self)
    {
        if (self == NULL)
        {
            return;
        }

        if (self->buffer != NULL)
        {
            free(self->buffer);
            self->buffer = NULL;
        }

        self->element_num = 0;
        free(self);
        self = NULL;
    }

    static struct array_s* array_new(size_t num, size_t element_size)
    {
        size_t buffer_size = num * element_size;
        struct array_s* ret = (struct array_s*)malloc(sizeof(struct array_s));
        if (ret == NULL)
        {
            return NULL;
        }

        memset(ret, 0, sizeof(*ret));
        ret->buffer = (char*)malloc(buffer_size);
        if (ret->buffer != NULL)
        {
            memset(ret->buffer, 0, buffer_size);
            ret->element_size = element_size;
            ret->element_num = num;
            ret->buffer_size = buffer_size;
        }
        else
        {
            array_delete(ret);
            ret = NULL;
        }

        return ret;
    }

    static char* array_at(struct array_s* self, size_t index)
    {
        char* ret = NULL;

        if (index < self->element_num)
        {
            ret = self->buffer + (index * self->element_size);
        }
        else
        {
            assert(false);
        }

        return ret;
    }

    static bool array_set(struct array_s* self, size_t index, const void* data)
    {
        char* old_data = array_at(self, index);

        if (old_data != NULL)
        {
            memcpy(old_data, data, self->element_size);
            return true;
        }
        else
        {
            return false;
        }
    }

    static bool array_increase(struct array_s* self, size_t increase_num)
    {
        size_t new_buffer_size = 0;
        char* new_buffer = NULL;

        if (increase_num == 0)
        {
            return false;
        }

        new_buffer_size = self->buffer_size + increase_num * self->element_size;
        new_buffer = (char*)malloc(new_buffer_size);

        if (new_buffer != NULL)
        {
            memset(new_buffer, 0, new_buffer_size);
            memcpy(new_buffer, self->buffer, self->buffer_size);
            free(self->buffer);
            self->buffer = new_buffer;
            self->element_num += increase_num;
            self->buffer_size = new_buffer_size;

            return true;
        }
        else
        {
            assert(false);
            return false;
        }
    }

    static size_t array_num(const struct array_s* self)
    {
        return self->element_num;
    }

} }