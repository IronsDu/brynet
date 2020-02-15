#pragma once

#include <cstdbool>
#include <cstdint>
#include <cstdlib>
#include <cstring>

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
        if (self == nullptr)
        {
            return;
        }

        if (self->data != nullptr)
        {
            free(self->data);
            self->data = nullptr;
        }

        free(self);
        self = nullptr;
    }

    static struct buffer_s* buffer_new(size_t buffer_size)
    {
        struct buffer_s* ret = (struct buffer_s*)malloc(sizeof(struct buffer_s));
        if (ret == nullptr)
        {
            return nullptr;
        }

        ret->data_len = 0;
        ret->read_pos = 0;
        ret->write_pos = 0;
        ret->data = (char*)malloc(sizeof(char) * buffer_size);

        if (ret->data != nullptr)
        {
            ret->data_len = buffer_size;
        }
        else
        {
            buffer_delete(ret);
            ret = nullptr;
        }

        return ret;
    }

    static size_t buffer_getreadvalidcount(struct buffer_s* self)
    {
        return self->write_pos - self->read_pos;
    }

    static void buffer_adjustto_head(struct buffer_s* self)
    {
        if (self->read_pos == 0)
        {
            return;
        }

        const auto len = buffer_getreadvalidcount(self);
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

    static bool buffer_addwritepos(struct buffer_s* self, size_t value)
    {
        const size_t temp = self->write_pos + value;
        if (temp <= self->data_len)
        {
            self->write_pos = temp;
            return true;
        }
        else
        {
            return false;
        }
    }

    static bool buffer_addreadpos(struct buffer_s* self, size_t value)
    {
        const size_t temp = self->read_pos + value;
        if (temp <= self->data_len)
        {
            self->read_pos = temp;
            return true;
        }
        else
        {
            return false;
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
            return nullptr;
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
            return nullptr;
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