#pragma once

#include <cstdbool>
#include <cstring>
#include <cstdlib>
#include <cassert>

namespace brynet { namespace base {

    struct array_s
    {
        void*   buffer;
        size_t  buffer_size;
        size_t  element_size;
        size_t  element_num;
    };

    static void array_delete(struct array_s* self)
    {
        if (self == nullptr)
        {
            return;
        }

        if (self->buffer != nullptr)
        {
            free(self->buffer);
            self->buffer = nullptr;
        }

        self->element_num = 0;
        free(self);
        self = nullptr;
    }

    static struct array_s* array_new(size_t num, size_t element_size)
    {
        auto ret = (struct array_s*)malloc(sizeof(struct array_s));
        if (ret == nullptr)
        {
            return nullptr;
        }

        const auto buffer_size = num * element_size;

        ret->buffer_size = 0;
        ret->element_size = 0;
        ret->element_num = 0;
        ret->buffer = malloc(buffer_size);

        if (ret->buffer != nullptr)
        {
            ret->element_size = element_size;
            ret->element_num = num;
            ret->buffer_size = buffer_size;
        }
        else
        {
            array_delete(ret);
            ret = nullptr;
        }

        return ret;
    }

    static void* array_at(struct array_s* self, size_t index)
    {
        void* ret = nullptr;

        if (index < self->element_num)
        {
            ret = (char*)(self->buffer) + (index * self->element_size);
        }
        else
        {
            assert(false);
        }

        return ret;
    }

    static bool array_set(struct array_s* self, size_t index, const void* data)
    {
        void* old_data = array_at(self, index);

        if (old_data != nullptr)
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
        if (increase_num == 0)
        {
            return false;
        }

        const auto new_buffer_size = self->buffer_size + increase_num * self->element_size;
        auto new_buffer = malloc(new_buffer_size);

        if (new_buffer != nullptr)
        {
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