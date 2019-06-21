#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include <brynet/net/poller.h>

#ifdef PLATFORM_WINDOWS
#define CHECK_READ_FLAG (POLLIN | POLLRDNORM | POLLRDBAND)
#define CHECK_WRITE_FLAG (POLLOUT | POLLWRNORM)
#define CHECK_ERROR_FLAG (POLLERR | POLLHUP)
#elif defined PLATFORM_LINUX || defined PLATFORM_DARWIN
#include <poll.h>
#define CHECK_READ_FLAG (POLLIN | POLLRDNORM | POLLRDBAND | POLLPRI)
#define CHECK_WRITE_FLAG (POLLOUT | POLLWRNORM | POLLWRBAND)
#define CHECK_ERROR_FLAG (POLLERR | POLLHUP)
#endif

struct poller_s
{
    struct pollfd* pollFds;
    int nfds;
    int limitSize;
};

static
void upstepPollfd(struct poller_s* self, int upSize)
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
struct pollfd* findPollfd(struct poller_s* self, sock fd)
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
void TryRemovePollFd(struct poller_s* self, sock fd)
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

struct poller_s*
ox_poller_new(void)
{
    struct poller_s* ret = (struct poller_s*)malloc(sizeof(struct poller_s));
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
ox_poller_delete(struct poller_s* self)
{
    free(self->pollFds);
    self->pollFds = NULL;
    self->nfds = 0;
    self->limitSize = 0;

    free(self);
    self = NULL;
}

void
ox_poller_add(struct poller_s* self, sock fd, int type)
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
ox_poller_del(struct poller_s* self, sock fd, int type)
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

void
ox_poller_remove(struct poller_s* self, sock fd)
{
    TryRemovePollFd(self, fd);
}

static bool
check_event(const struct pollfd* pf, enum CheckType type)
{
    if (pf == NULL)
    {
        return false;
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

void
ox_poller_visitor(struct poller_s* self, enum CheckType type, struct stack_s* result)
{
    int i = 0;
    for (; i < self->nfds; i++)
    {
        if (check_event(self->pollFds+i, type))
        {
            ox_stack_push(result, &self->pollFds[i].fd);
        }
    }
}

int 
ox_poller_poll(struct poller_s* self, long overtime)
{
#ifdef PLATFORM_WINDOWS
    int ret = WSAPoll(&self->pollFds[0], self->nfds, overtime);
#elif defined PLATFORM_LINUX || defined PLATFORM_DARWIN
    int ret = poll(self->pollFds, self->nfds, overtime);
#endif

    if(ret == SOCKET_ERROR)
    {
        ret = (sErrno != S_EINTR) ? -1 : 0;
    }

    return ret;
}

bool 
ox_poller_check(struct poller_s* self, sock fd, enum CheckType type)
{
    const struct pollfd* pf = findPollfd(self, fd);
    if (pf == NULL)
    {
        return false;
    }
    return check_event(pf, type);
}
