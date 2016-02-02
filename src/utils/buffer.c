#include <stdlib.h>
#include <string.h>

#include "buffer.h"

struct buffer_s
{
    char*   data;
    size_t data_len;

    size_t write_pos;
    size_t read_pos;
};

void 
ox_buffer_delete(struct buffer_s* self)
{
    if(self != NULL)
    {
        if(self->data != NULL)
        {
            free(self->data);
            self->data = NULL;
        }

        free(self);
        self = NULL;
    }
}

struct buffer_s* 
ox_buffer_new(size_t buffer_size)
{
    struct buffer_s* ret = (struct buffer_s*)malloc(sizeof(struct buffer_s));

    if(ret != NULL)
    {
        if((ret->data = (char*)malloc(sizeof(char)*buffer_size)) != NULL)
        {
            ret->data_len = buffer_size;
            ret->read_pos = 0;
            ret->write_pos = 0;
        }
        else
        {
            ox_buffer_delete(ret);
            ret = NULL;
        }
    }

    return ret;
}

void 
ox_buffer_adjustto_head(struct buffer_s* self)
{
    size_t len = 0;

    if(self->read_pos > 0)
    {
        len = ox_buffer_getreadvalidcount(self);
        if(len > 0 )
        {
            memmove(self->data, self->data+self->read_pos, len);
        }
        
        self->read_pos = 0;
        self->write_pos = len;
    }
}

void 
ox_buffer_init(struct buffer_s* self)
{
    self->read_pos = 0;
    self->write_pos = 0;
}

size_t
ox_buffer_getwritepos(struct buffer_s* self)
{
    return self->write_pos;
}

size_t
ox_buffer_getreadpos(struct buffer_s* self)
{
    return self->read_pos;
}

void 
ox_buffer_addwritepos(struct buffer_s* self, size_t value)
{
    size_t temp = self->write_pos + value;
    if(temp <= self->data_len)
    {
        self->write_pos = temp;
    }
}

void 
ox_buffer_addreadpos(struct buffer_s* self, size_t value)
{
    size_t temp = self->read_pos + value;
    if(temp <= self->data_len)
    {
        self->read_pos = temp;
    }
}

size_t
ox_buffer_getreadvalidcount(struct buffer_s* self)
{
    return self->write_pos - self->read_pos;
}

size_t
ox_buffer_getwritevalidcount(struct buffer_s* self)
{
    return self->data_len - self->write_pos;
}

size_t
ox_buffer_getsize(struct buffer_s* self)
{
    return self->data_len;
}

char* 
ox_buffer_getwriteptr(struct buffer_s* self)
{
    if(self->write_pos < self->data_len)
    {
        return self->data + self->write_pos;
    }
    else
    {
        return NULL;
    }
}

char* 
ox_buffer_getreadptr(struct buffer_s* self)
{
    if(self->read_pos < self->data_len)
    {
        return self->data + self->read_pos;
    }
    else
    {
        return NULL;
    }
}

bool 
ox_buffer_write(struct buffer_s* self, const char* data, size_t len)
{
    bool write_ret = true;

    if(ox_buffer_getwritevalidcount(self) >= len)
    {
        /*  直接写入    */
        memcpy(ox_buffer_getwriteptr(self), data, len);
        ox_buffer_addwritepos(self, len);
    }
    else
    {
        size_t left_len = self->data_len - ox_buffer_getreadvalidcount(self);
        if(left_len >= len)
        {
            ox_buffer_adjustto_head(self);
            ox_buffer_write(self, data, len);
        }
        else
        {
            write_ret = false;
        }
    }

    return write_ret;
}
