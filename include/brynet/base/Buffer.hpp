#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

namespace brynet { namespace base {

    struct buffer_s
    {
        char*   data;
        size_t  data_len;

        size_t  write_pos;
        size_t  read_pos;
    };

    static void buffer_delete(struct buffer_s* self)
    {
        if (self == NULL)
        {
            return;
        }

        if (self->data != NULL)
        {
            free(self->data);
            self->data = NULL;
        }

        free(self);
        self = NULL;
    }

    static struct buffer_s* buffer_new(size_t buffer_size)
    {
        struct buffer_s* ret = (struct buffer_s*)malloc(sizeof(struct buffer_s));
        if (ret == NULL)
        {
            return NULL;
        }

        if ((ret->data = (char*)malloc(sizeof(char) * buffer_size)) != NULL)
        {
            ret->data_len = buffer_size;
            ret->read_pos = 0;
            ret->write_pos = 0;
        }
        else
        {
            buffer_delete(ret);
            ret = NULL;
        }

        return ret;
    }

    static size_t buffer_getreadvalidcount(struct buffer_s* self)
    {
        return self->write_pos - self->read_pos;
    }

    static void buffer_adjustto_head(struct buffer_s* self)
    {
        size_t len = 0;

        if (self->read_pos <= 0)
        {
            return;
        }

        len = buffer_getreadvalidcount(self);
        if (len > 0)
        {
            memmove(self->data, self->data + self->read_pos, len);
        }

        self->read_pos = 0;
        self->write_pos = len;
    }

    static void buffer_init(struct buffer_s* self)
    {
        self->read_pos = 0;
        self->write_pos = 0;
    }

    static size_t buffer_getwritepos(struct buffer_s* self)
    {
        return self->write_pos;
    }

    static size_t buffer_getreadpos(struct buffer_s* self)
    {
        return self->read_pos;
    }

    static void buffer_addwritepos(struct buffer_s* self, size_t value)
    {
        size_t temp = self->write_pos + value;
        if (temp <= self->data_len)
        {
            self->write_pos = temp;
        }
    }

    static void buffer_addreadpos(struct buffer_s* self, size_t value)
    {
        size_t temp = self->read_pos + value;
        if (temp <= self->data_len)
        {
            self->read_pos = temp;
        }
    }

    static size_t buffer_getwritevalidcount(struct buffer_s* self)
    {
        return self->data_len - self->write_pos;
    }

    static size_t buffer_getsize(struct buffer_s* self)
    {
        return self->data_len;
    }

    static char* buffer_getwriteptr(struct buffer_s* self)
    {
        if (self->write_pos < self->data_len)
        {
            return self->data + self->write_pos;
        }
        else
        {
            return NULL;
        }
    }

    static char* buffer_getreadptr(struct buffer_s* self)
    {
        if (self->read_pos < self->data_len)
        {
            return self->data + self->read_pos;
        }
        else
        {
            return NULL;
        }
    }


    static bool buffer_write(struct buffer_s* self, const char* data, size_t len)
    {
        bool write_ret = true;

        if (buffer_getwritevalidcount(self) >= len)
        {
            /*  Ö±½ÓÐ´Èë    */
            memcpy(buffer_getwriteptr(self), data, len);
            buffer_addwritepos(self, len);
        }
        else
        {
            size_t left_len = self->data_len - buffer_getreadvalidcount(self);
            if (left_len >= len)
            {
                buffer_adjustto_head(self);
                buffer_write(self, data, len);
            }
            else
            {
                write_ret = false;
            }
        }

        return write_ret;
    }

} }