#include "platform.h"

#ifdef PLATFORM_LINUX
#include <sys/epoll.h>
#include <signal.h>
    #ifndef EPOLLRDHUP
    #define EPOLLRDHUP (0x2000)
    #endif
#include <sys/uio.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "stack.h"
#include "socketlibtypes.h"
#include "socketlibfunction.h"
#include "server_private.h"
#include "buffer.h"
#include "systemlib.h"

#define MAX_EVENTS (200)

struct epollserver_s;

enum session_status
{
    session_status_none,
    session_status_connect,
    session_status_halfclose,
};

struct session_s
{
    struct epollserver_s*    server;
    sock                    fd;

    bool                    writeable;

    bool                    haveleftdata;   /*  逻辑层没有发送完数据标志    */

    struct buffer_s*        recv_buffer;

    enum session_status     status;
    void*                   ud;
};

struct epollserver_s
{
    struct server_s base;

    int session_recvbuffer_size;
    int session_sendbuffer_size;

    int epoll_fd;
};

#ifdef PLATFORM_LINUX
static void
epoll_add_event(int epoll_fd, sock fd, struct session_s* client, uint32_t events)
{
    struct epoll_event ev = { 0, { 0 }};
    ev.events = events;
    ev.data.ptr = client;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev);
}
#endif

static struct session_s* epoll_session_malloc(struct epollserver_s* epollserver, sock fd)
{
    struct session_s* session = (struct session_s*)malloc(sizeof *session);
    session->recv_buffer = ox_buffer_new(epollserver->session_recvbuffer_size);

    ox_socket_nonblock(fd);
    session->writeable = true;
    session->haveleftdata = false;
    session->fd = fd;
    session->ud = NULL;
    session->server = epollserver;

    return session;
}

static bool
epoll_handle_newclient(struct epollserver_s* epollserver, void* ud, sock client_fd)
{
    bool result = false;

    struct session_s*   session = epoll_session_malloc(epollserver, client_fd);

    if(session != NULL)
    {
        struct server_s* server = &epollserver->base;
        session->ud = ud;
        session->status = session_status_connect;

        (*(server->logic_on_enter))(server, session->ud, session);

#ifdef PLATFORM_LINUX
        epoll_add_event(epollserver->epoll_fd, client_fd, session, EPOLLET | EPOLLIN | EPOLLOUT | EPOLLRDHUP);
#endif
        result = true;
    }

    return result;
}

static void
epollserver_halfclose_session(struct epollserver_s* epollserver, struct session_s* session)
{
    if(session->status == session_status_connect)
    {
    #ifdef PLATFORM_LINUX
        epoll_ctl(epollserver->epoll_fd, EPOLL_CTL_DEL, session->fd, NULL);
        session->status = session_status_halfclose;
    #endif
    }
}

static void
epollserver_handle_sessionclose(struct epollserver_s* epollserver, struct session_s* session)
{
    if(SOCKET_ERROR != session->fd)
    {
        epollserver_halfclose_session(epollserver, session);
        ox_socket_close(session->fd);
        session->fd = SOCKET_ERROR;

        session->status = session_status_none;
    }

    if(session->recv_buffer != NULL)
    {
        ox_buffer_delete(session->recv_buffer);
        session->recv_buffer = NULL;
    }

    free(session);
}

static void
epoll_handle_onoutevent(struct session_s* session)
{
    struct server_s* server = &session->server->base;
    session->writeable = true;
    if(server->logic_on_sendfinish != NULL)
    {
        (server->logic_on_sendfinish)(server, session->ud, 0);
    }
}

static void
epoll_recvdata_callback(void* msg)
{
    struct session_s*   session = (struct session_s*)msg;
    struct buffer_s*    recv_buffer = session->recv_buffer;
    struct server_s*    server = &(session->server->base);
    int all_recvlen = 0;
    bool is_close = false;

    bool proc_begin = false;

    if(session->fd == SOCKET_ERROR)
    {
        return;
    }

procbegin:
    proc_begin = false;
    all_recvlen = 0;

    for(;;)
    {
        int can_recvlen = 0;
        int recv_len = 0;

        if(ox_buffer_getwritevalidcount(recv_buffer) <= 0)
        {
            ox_buffer_adjustto_head(recv_buffer);
        }

        can_recvlen = ox_buffer_getwritevalidcount(recv_buffer);

        if(can_recvlen > 0)
        {
            recv_len = recv(session->fd, ox_buffer_getwriteptr(recv_buffer), can_recvlen, 0);
            if(recv_len == 0)
            {
                is_close = true;
            }
            else if(SOCKET_ERROR == recv_len)
            {
                is_close = (S_EWOULDBLOCK != sErrno);
            }
            else
            {
                all_recvlen += recv_len;
                ox_buffer_addwritepos(recv_buffer, recv_len);

                if(recv_len == can_recvlen)
                {
                    proc_begin = true;
                }
            }
        }

        break;
    }

    if(is_close)
    {
        epollserver_halfclose_session(session->server, session);
        (*server->logic_on_close)(server, session->ud);
    }
    else if(all_recvlen > 0)
    {
        int proc_len = (*server->logic_on_recved)(server, session->ud, ox_buffer_getreadptr(recv_buffer), ox_buffer_getreadvalidcount(recv_buffer));
        ox_buffer_addreadpos(recv_buffer, proc_len);
    }

    if(proc_begin && session->fd != SOCKET_ERROR)   /*  确保逻辑层在logic_on_recved中没有调用关闭session */
    {
        goto procbegin;
    }
}

static void
epollserver_start_callback(
    struct server_s* self,
    logic_on_enter_handle enter_pt,
    logic_on_disconnection_handle on_disconnection_pt,
    logic_on_recved_handle   recved_pt,
    logic_on_sendfinish_handle sendfinish_pt,
    logic_on_close_completed  closecompleted_callback
    )
{
    struct epollserver_s* epollserver = (struct epollserver_s*)self;

    self->logic_on_enter = enter_pt;
    self->logic_on_disconnection = on_disconnection_pt;
    self->logic_on_recved = recved_pt;
    self->logic_on_sendfinish = sendfinish_pt;
    self->logic_on_closecompleted = closecompleted_callback;

    epollserver->epoll_fd = epoll_create(1);
}

static void
epollserver_stop_callback(struct server_s* self)
{
    struct epollserver_s* epollserver = (struct epollserver_s*)self;

    if(epollserver->epoll_fd != SOCKET_ERROR)
    {
        ox_socket_close(epollserver->epoll_fd);
        epollserver->epoll_fd = SOCKET_ERROR;
    }

    free(epollserver);
}

static void
epollserver_poll(struct server_s* self, int64_t timeout)
{
    #ifdef PLATFORM_LINUX
    struct epollserver_s* epollserver = (struct epollserver_s*)self;
    int epollfd = epollserver->epoll_fd;
    struct epoll_event events[MAX_EVENTS];

    int i = 0;
    int nfds = epoll_wait(epollfd, events, MAX_EVENTS, timeout);

    for(i = 0; i < nfds; ++i)
    {
        struct session_s*   session = (struct session_s*)(events[i].data.ptr);
        uint32_t event_data = events[i].events;

        if(session->status == session_status_connect)
        {
            if(event_data & EPOLLRDHUP)
            {
                /*  可能数据和close一起触发,使用recv尝试读取数据且检测断开    */
                epoll_recvdata_callback(session);
                if(session->status == session_status_connect)
                {
                    /*  再次检测它的状态是否由recv逻辑进行了关闭，避免由于一些情况导致并没有进行关闭处理    */
                    epollserver_halfclose_session(session->server, session);
                    (*self->logic_on_close)(self, session->ud);
                }
            }
            else
            {
                if(event_data & EPOLLIN)
                {
                    epoll_recvdata_callback(session);
                }

                if(event_data & EPOLLOUT)
                {
                    epoll_handle_onoutevent(session);
                }
            }
        }
    }

    #endif
}

static void
epollserver_closesession_callback(struct server_s* self, void* handle)
{
    struct epollserver_s* epollserver = (struct epollserver_s*)self;

    struct session_s* session = (struct session_s*)handle;
    void* ud = session->ud;
    epollserver_handle_sessionclose(epollserver, session);
    (self->logic_on_closecompleted)(self, ud);
}

static bool
epollserver_register(struct server_s* self, void* ud, int fd)
{
    return epoll_handle_newclient((struct epollserver_s*)self, ud, fd);
}

static int
epollserver_sendv_callback(struct server_s* self, void* handle, const char* datas[], const int* lens, int num)
{
    int send_len = 0;

    if(num > 0 && num <= MAX_SENDBUF_NUM)
    {
        struct session_s* session = (struct session_s*)handle;
        if(session->status == session_status_connect && session->writeable)
        {
            struct iovec iov[MAX_SENDBUF_NUM];
            int i = 0;
            int ready_send_len = 0;

            for(; i < num; ++i)
            {
                iov[i].iov_base = (char*)datas[i];
                iov[i].iov_len = lens[i];
                ready_send_len += lens[i];
            }

            send_len = writev(session->fd, iov, num);

            if(send_len == -1)
            {
                if(sErrno == S_EWOULDBLOCK)
                {
                    send_len = 0;
                }
            }

            session->writeable = (ready_send_len == send_len);
        }
    }

    return send_len;
}

struct server_s*
epollserver_create(
    int session_recvbuffer_size,
    int session_sendbuffer_size,
    void*   ext)
{
    struct epollserver_s* epollserver = (struct epollserver_s*)malloc(sizeof(*epollserver));
    memset(epollserver, 0, sizeof(*epollserver));

    epollserver->base.start_callback = epollserver_start_callback;
    epollserver->base.poll_callback = epollserver_poll;
    epollserver->base.stop_callback = epollserver_stop_callback;
    epollserver->base.closesession_callback = epollserver_closesession_callback;
    epollserver->base.register_callback = epollserver_register;
    epollserver->base.sendv_callback = epollserver_sendv_callback;
    epollserver->base.ext = ext;

    epollserver->session_recvbuffer_size = session_recvbuffer_size;
    epollserver->session_sendbuffer_size = session_sendbuffer_size;

    epollserver->epoll_fd = SOCKET_ERROR;

    /*  去掉保护    signal(SIGPIPE, SIG_IGN);   */
    return &epollserver->base;
}
