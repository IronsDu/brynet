#pragma once

#include <cstdlib>
#include <cstdio>
#include <cassert>
#include <cstdbool>
#include <string>

#include <brynet/net/SocketLibTypes.hpp>
#include <brynet/base/Stack.hpp>

#if defined BRYNET_PLATFORM_LINUX || defined BRYNET_PLATFORM_DARWIN
#include <poll.h>
#endif

namespace brynet { namespace base {

    #ifdef BRYNET_PLATFORM_WINDOWS
    const static int CHECK_READ_FLAG = (POLLIN | POLLRDNORM | POLLRDBAND);
    const static int CHECK_WRITE_FLAG = (POLLOUT | POLLWRNORM);
    const static int CHECK_ERROR_FLAG = (POLLERR | POLLHUP);
    #elif defined BRYNET_PLATFORM_LINUX || defined BRYNET_PLATFORM_DARWIN
    const static int CHECK_READ_FLAG = (POLLIN | POLLRDNORM | POLLRDBAND | POLLPRI);
    const static int CHECK_WRITE_FLAG = (POLLOUT | POLLWRNORM | POLLWRBAND);
    const static int CHECK_ERROR_FLAG = (POLLERR | POLLHUP);
    #endif

    enum CheckType
    {
        ReadCheck = 0x1,
        WriteCheck = 0x2,
        ErrorCheck = 0x4,
    };

    struct poller_s
    {
        struct pollfd* pollFds;
        int nfds;
        int limitSize;
    };

    static void upstep_pollfd(struct poller_s* self, int upSize)
    {
        if (upSize <= 0)
        {
            return;
        }

        struct pollfd* newPollfds = (struct pollfd*)malloc(
            sizeof(struct pollfd) * (self->limitSize + upSize));
        if (newPollfds == nullptr)
        {
            return;
        }

        if (self->pollFds != nullptr)
        {
            memcpy(newPollfds, self->pollFds, sizeof(struct pollfd) * self->nfds);
            free(self->pollFds);
            self->pollFds = nullptr;
        }
        self->pollFds = newPollfds;
        self->limitSize += upSize;
    }

    static struct pollfd* find_pollfd(struct poller_s* self, BrynetSocketFD fd)
    {
        for (int i = 0; i < self->nfds; i++)
        {
            if (self->pollFds[i].fd == fd)
            {
                return self->pollFds + i;
            }
        }

        return nullptr;
    }

    static void try_remove_pollfd(struct poller_s* self, BrynetSocketFD fd)
    {
        int pos = -1;
        for (int i = 0; i < self->nfds; i++)
        {
            if (self->pollFds[i].fd == fd)
            {
                pos = i;
                break;
            }
        }

        if (pos != -1)
        {
            memmove(self->pollFds + pos, 
                self->pollFds + pos + 1, 
                sizeof(struct pollfd) * (self->nfds - pos - 1));
            self->nfds--;
            assert(self->nfds >= 0);
        }
    }

    static struct poller_s* poller_new(void)
    {
        struct poller_s* ret = (struct poller_s*)malloc(sizeof(struct poller_s));
        if (ret != nullptr)
        {
            ret->pollFds = NULL;
            ret->limitSize = 0;
            ret->nfds = 0;
            upstep_pollfd(ret, 1024);
        }

        return ret;
    }

    static void poller_delete(struct poller_s* self)
    {
        free(self->pollFds);
        self->pollFds = nullptr;
        self->nfds = 0;
        self->limitSize = 0;

        free(self);
        self = nullptr;
    }

    static void poller_add(struct poller_s* self, BrynetSocketFD fd, int type)
    {
        if (self->limitSize == self->nfds)
        {
            upstep_pollfd(self, 128);
        }

        if (self->limitSize <= self->nfds)
        {
            return;
        }

        struct pollfd* pf = find_pollfd(self, fd);
        if (pf == nullptr)
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

    static void poller_del(struct poller_s* self, BrynetSocketFD fd, int type)
    {
        struct pollfd* pf = find_pollfd(self, fd);
        if (pf == nullptr)
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
            try_remove_pollfd(self, fd);
        }
    }

    static void poller_remove(struct poller_s* self, BrynetSocketFD fd)
    {
        try_remove_pollfd(self, fd);
    }

    static bool check_event(const struct pollfd* pf, enum CheckType type)
    {
        if (pf == nullptr)
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

    static void poller_visitor(struct poller_s* self, 
        enum CheckType type, 
        struct stack_s* result)
    {
        for (int i = 0; i < self->nfds; i++)
        {
            if (check_event(self->pollFds + i, type))
            {
                stack_push(result, &self->pollFds[i].fd);
            }
        }
    }

    static int poller_poll(struct poller_s* self, long overtime)
    {
#ifdef BRYNET_PLATFORM_WINDOWS
        int ret = WSAPoll(&self->pollFds[0], self->nfds, overtime);
#elif defined BRYNET_PLATFORM_LINUX || defined BRYNET_PLATFORM_DARWIN
        int ret = poll(self->pollFds, self->nfds, overtime);
#endif

        if (ret == BRYNET_SOCKET_ERROR)
        {
            ret = (BRYNET_ERRNO != BRYNET_EINTR) ? -1 : 0;
        }

        return ret;
    }

    static bool poller_check(struct poller_s* self, BrynetSocketFD fd, enum CheckType type)
    {
        const struct pollfd* pf = find_pollfd(self, fd);
        if (pf == NULL)
        {
            return false;
        }
        return check_event(pf, type);
    }

} }
