#include <stdlib.h>
#include <string.h>

#include "array.h"
#include "fdset.h"

struct fdset_s
{
    fd_set  read;
    fd_set  write;
    fd_set  error;

    fd_set  read_result;
    fd_set  write_result;
    fd_set  error_result;

    #if defined PLATFORM_LINUX
    struct array_s*    fds;
    sock        max_fd;
    sock        push_max_fd;
    #endif
};

struct fdset_s* 
ox_fdset_new(void)
{
    struct fdset_s* ret = (struct fdset_s*)malloc(sizeof(struct fdset_s));
    if(ret != NULL)
    {
        FD_ZERO(&ret->read);
        FD_ZERO(&ret->write);
        FD_ZERO(&ret->error);

        FD_ZERO(&ret->read_result);
        FD_ZERO(&ret->write_result);
        FD_ZERO(&ret->error_result);



        #ifdef PLATFORM_LINUX
        ret->max_fd = SOCKET_ERROR;
        ret->fds = NULL;
        ret->push_max_fd = 0;
        #endif
    }

    return ret;
}

fd_set* 
ox_fdset_getresult(struct fdset_s* self, enum CheckType type)
{
    fd_set* ret = NULL;
    if(ReadCheck == type)
    {
        ret = &self->read_result;
    }
    else if(WriteCheck == type)
    {
        ret = &self->write_result;
    }
    else
    {
        ret = &self->error_result;
    }

    return ret;
}

void 
ox_fdset_delete(struct fdset_s* self)
{
    #ifdef PLATFORM_LINUX
    self->max_fd = SOCKET_ERROR;
    if(self->fds != NULL)
    {
        ox_array_delete(self->fds);
        self->fds = NULL;
    }
    #endif

    free(self);
    self = NULL;
}

#if defined PLATFORM_LINUX

static void 
fdset_push(struct fdset_s* self, sock fd)
{
    if(self->fds == NULL)
    {
        self->fds = ox_array_new(fd, sizeof(bool));
    }

    int fd_num = ox_array_num(self->fds);
    if(fd_num <= fd)
    {
        ox_array_increase(self->fds, fd_num);
    }

    bool data = true;
    ox_array_set(self->fds, fd, &data);

    if(fd > self->max_fd)
    {
        self->max_fd = fd;
    }

    int i = self->push_max_fd+1;
    data = false;
    for(; i < fd; ++i)
    {
        ox_array_set(self->fds, i, &data);
    }

    if(fd > self->push_max_fd)
    {
        self->push_max_fd = fd;
    }
}

static void 
fdset_erase(struct fdset_s* self, sock fd)
{
    bool data = false;
    ox_array_set(self->fds, fd, &data);

    if(fd == self->max_fd)
    {
        self->max_fd = SOCKET_ERROR;
        int i = fd;
        for(; i >= 0; --i)
        {
            if(*(bool*)ox_array_at(self->fds, i))
            {
                self->max_fd = i;
                break;
            }

        }
    }
}

#endif

void 
ox_fdset_add(struct fdset_s* self, sock fd, int type)
{
    if(type & ReadCheck)
    {
        FD_SET(fd, &self->read);
    }

    if(type & WriteCheck)
    {
        FD_SET(fd, &self->write);
    }

    if(type & ErrorCheck)
    {
        FD_SET(fd, &self->error);
    }

    #if defined PLATFORM_LINUX
    fdset_push(self, fd);
    #endif
}

void 
ox_fdset_del(struct fdset_s* self, sock fd, int type)
{
    if(type & ReadCheck)
    {
        FD_CLR(fd, &self->read);
    }

    if(type & WriteCheck)
    {
        FD_CLR(fd, &self->write);
    }

    if(type & ErrorCheck)
    {
        FD_CLR(fd, &self->error);
    }

    #if defined PLATFORM_LINUX
    if(!FD_ISSET(fd, &self->write) && !FD_ISSET(fd, &self->read) && !FD_ISSET(fd, &self->error))
    {
        fdset_erase(self, fd);
    }
    #endif
}

int 
ox_fdset_poll(struct fdset_s* self, long overtime)
{
    struct timeval t = { 0, overtime*1000};
    int ret = 0;

    self->read_result = self->read;
    self->write_result = self->write;
    self->error_result = self->error;

    #if defined PLATFORM_WINDOWS
    if(self->read.fd_count != 0 || self->write.fd_count != 0 || self->error.fd_count != 0)
    {
        ret = select(0, &self->read_result, &self->write_result, &self->error_result, &t);
    }
    #else
    int max_fd = self->max_fd;
    if(SOCKET_ERROR != max_fd)
    {
        ret = select(max_fd + 1, &self->read_result, &self->write_result, &self->error_result, &t);
    }
    #endif

    if(ret == SOCKET_ERROR)
    {
        ret = (sErrno != S_EINTR) ? -1 : 0;
    }

    return ret;
}

bool 
ox_fdset_check(struct fdset_s* self, sock fd, enum CheckType type)
{
    bool active = false;

    do
    {
        if(type & ReadCheck)
        {
            if((active = FD_ISSET(fd, &self->read_result)))
            {
                break;
            }
        }

        if(type & WriteCheck)
        {
            if((active = FD_ISSET(fd, &self->write_result)))
            {
                break;
            }
        }

        if(type & ErrorCheck)
        {
            if((active = FD_ISSET(fd, &self->error_result)))
            {
                break;
            }
        }
    }   while(0);

    return active;
}
