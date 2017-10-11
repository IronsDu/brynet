#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include <brynet/net/fdset.h>

#if defined PLATFORM_LINUX
#include <poll.h>
#define CHECK_READ_FLAG (POLLIN | POLLRDNORM | POLLRDBAND | POLLPRI)
#define CHECK_WRITE_FLAG (POLLOUT | POLLWRNORM | POLLWRBAND)
#define CHECK_ERROR_FLAG (POLLERR | POLLHUP)
#else
#define CHECK_READ_FLAG (POLLIN | POLLRDNORM | POLLRDBAND)
#define CHECK_WRITE_FLAG (POLLOUT | POLLWRNORM)
#define CHECK_ERROR_FLAG (POLLERR | POLLHUP)
#endif

struct fdset_s
{
    struct pollfd* pollFds;
    int nfds;
    int limitSize;
};

static
void upstepPollfd(struct fdset_s* self, int upSize)
{
    if (upSize <= 0)
    {
        return;
    }

    struct pollfd* newPollfds = (struct pollfd*)malloc(sizeof(struct pollfd)*(self->limitSize + upSize));
    if (newPollfds == NULL)
    {
        return;
    }

    if (self->pollFds != NULL)
    {
        memcpy(newPollfds, self->pollFds, sizeof(struct pollfd)*self->nfds);
        free(self->pollFds);
        self->pollFds = NULL;
    }
    self->pollFds = newPollfds;
    self->limitSize += upSize;
}

static
struct pollfd* findPollfd(struct fdset_s* self, sock fd)
{
    int i = 0;
    for (; i < self->nfds; i++)
    {
        if(self->pollFds[i].fd == fd)
        {
            return self->pollFds+i;
        }
    }

    return NULL;
}

static
void TryRemovePollFd(struct fdset_s* self, sock fd)
{
    int pos = -1;
    int i = 0;
    for (; i < self->nfds; i++)
    {
        if(self->pollFds[i].fd == fd)
        {
            pos = i;
            break;
        }
    }

    if (pos != -1)
    {
        memmove(self->pollFds + pos, self->pollFds + pos + 1, sizeof(struct pollfd)*(self->nfds - pos - 1));
        self->nfds--;
        assert(self->nfds >= 0);
    }
}

struct fdset_s*
ox_fdset_new(void)
{
    struct fdset_s* ret = (struct fdset_s*)malloc(sizeof(struct fdset_s));
    if(ret != NULL)
    {
        ret->pollFds = NULL;
        ret->limitSize = 0;
        ret->nfds = 0;
        upstepPollfd(ret, 1024);
    }

    return ret;
}

void 
ox_fdset_delete(struct fdset_s* self)
{
    free(self->pollFds);
    self->pollFds = NULL;
    self->nfds = 0;
    self->limitSize = 0;

    free(self);
    self = NULL;
}

void
ox_fdset_add(struct fdset_s* self, sock fd, int type)
{
    if (self->limitSize == self->nfds)
    {
        upstepPollfd(self, 128);
    }

    if (self->limitSize <= self->nfds)
    {
        return;
    }

    struct pollfd* pf = findPollfd(self, fd);
    if (pf == NULL)
    {
        /*real add*/
        pf = self->pollFds + self->nfds;
        pf->events = 0;
        pf->fd = fd;

        self->nfds++;
    }

    if (type & ReadCheck)
    {
        pf->events |= CHECK_READ_FLAG;
    }

    if (type & WriteCheck)
    {
        pf->events |= CHECK_WRITE_FLAG;
    }

    if (type & ErrorCheck)
    {
        //pf->events |= CHECK_ERROR_FLAG;   TODO::on windows, not supports
    }
}

void
ox_fdset_del(struct fdset_s* self, sock fd, int type)
{
    struct pollfd* pf = findPollfd(self, fd);
    if (pf == NULL)
    {
        return;
    }

    if (type & ReadCheck)
    {
        pf->events &= ~CHECK_READ_FLAG;
    }

    if (type & WriteCheck)
    {
        pf->events &= ~CHECK_WRITE_FLAG;
    }

    if (type & ErrorCheck)
    {
        pf->events &= ~CHECK_ERROR_FLAG;
    }

    if (pf->events == 0)
    {
        TryRemovePollFd(self, fd);
    }
}

int 
ox_fdset_poll(struct fdset_s* self, long overtime)
{
#if defined PLATFORM_WINDOWS
    int ret = WSAPoll(&self->pollFds[0], self->nfds, overtime);
#else
    int ret = poll(self->pollFds, self->nfds, overtime);
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
    int i = 0;
    for (; i < self->nfds; i++)
    {
        const struct pollfd* pf = self->pollFds + i;
        if (pf->fd != fd)
        {
            continue;
        }

        if ((type & ReadCheck) &&
            (pf->revents & CHECK_READ_FLAG))
        {
            return true;
        }
        else if ((type & WriteCheck) &&
                (pf->revents & CHECK_WRITE_FLAG))
        {
            return true;
        }
        else if ((type & ErrorCheck) &&
                (pf->revents & CHECK_ERROR_FLAG))
        {
            return true;
        }
        else
        {
            return false;
        }
    }

    return false;
}
