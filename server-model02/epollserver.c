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
    int index;
    sock fd;

    bool    writeable;
    
    bool    haveleftdata;   /*  逻辑层没有发送完数据标志    */
    
    struct buffer_s*    send_buffer;
    struct buffer_s*    recv_buffer;
    
    enum session_status status;
};

struct epollserver_s
{
    struct server_s base;

    int session_recvbuffer_size;
    int session_sendbuffer_size;

    int max_num;
    struct session_s*   sessions;

    struct stack_s* freelist;
    int freelist_num;
    
    int epoll_fd;
};

#ifdef PLATFORM_LINUX
static void
epoll_add_event(int epoll_fd, sock fd, struct session_s* client, uint32_t events)
{
    struct epoll_event ev = { 0, { 0 }};
    ev.events = events;

    ev.data.fd = fd;
    ev.data.ptr = client;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev);
}
#endif

static bool
epoll_handle_newclient(struct epollserver_s* epollserver, sock client_fd)
{
    bool result = false;
    
    if(epollserver->freelist_num > 0)
    {
        struct session_s*   session = NULL;

        {
            struct session_s** ppsession = (struct session_s**)ox_stack_popback(epollserver->freelist);
            if(ppsession != NULL)
            {
                session = *ppsession;
                epollserver->freelist_num--;
            }
        }

        if(session != NULL)
        {
            struct server_s* server = &epollserver->base;
            
            session->fd = client_fd;
            ox_buffer_init(session->recv_buffer);
            ox_buffer_init(session->send_buffer);
            ox_socket_nonblock(client_fd);
            session->writeable = true;
            session->haveleftdata = false;
            (*(server->logic_on_enter))(server, session->index);
            
            #ifdef PLATFORM_LINUX
            epoll_add_event(epollserver->epoll_fd, client_fd, session, EPOLLET | EPOLLIN | EPOLLOUT | EPOLLRDHUP);
            #endif
            
            session->status = session_status_connect;
            result = true;
        }
        else
        {
            printf("没有可用资源\n");
        }
    }
    else
    {
        printf("没有可用资源\n");
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
        ox_stack_push(epollserver->freelist, &session);
        epollserver->freelist_num++;
        
        session->status = session_status_none;
    }
}

/*  发送内置缓冲区未发送的数据 */
static void
epollserver_send_olddata(struct session_s* session)
{
    int send_len = 0;
    struct buffer_s*    send_buffer = session->send_buffer;
    int oldlen = ox_buffer_getreadvalidcount(send_buffer);
        
    if(oldlen > 0)
    {
        send_len = ox_socket_send(session->fd, ox_buffer_getreadptr(send_buffer), oldlen);

        if(send_len > 0)
        {
            ox_buffer_addwritepos(send_buffer, send_len);
        }
        
        session->writeable = (oldlen == oldlen);
    }
    
    return;
}

/*  优先发送内置缓冲区未发送的数据,然后发送指定数据 */
static int
epollserver_senddata(struct session_s* session, const char* data, int len)
{
    int send_len = 0;
    struct buffer_s*    send_buffer = session->send_buffer;
    
    if(data == NULL || len == 0)
    {
        return 0;
    }
    
    if(session->writeable)    /*  如果可写则发送内置缓冲区数据,然后仍然可写则继续发送指定数据  */
    {
        epollserver_send_olddata(session);
        
        if(session->writeable)
        {
            send_len = ox_socket_send(session->fd, data, len);
        }
        
        if(send_len < len)
        {
            session->writeable = false;  /*  设置为不可写  */
        }
    }
    
    if(send_len < len)  /*  如果发送到socket的长度小于预发送的长度，则copy到session内置缓冲区  */
    {
        if(ox_buffer_write(send_buffer, data+send_len, len-send_len))
        {
            send_len = len;
        }
    }
    
    /*  如果没有完全发送完数据则设置标志    */
    session->haveleftdata = (send_len < len);
    
    return send_len;
}

static void
epoll_handle_onoutevent(struct session_s* session)
{
    struct server_s* server = &session->server->base;
    session->writeable = true;
    if(server->logic_on_sendfinish != NULL)
    {
        (server->logic_on_sendfinish)(server, session->index, 0);
    }
    else
    { 
        if(ox_buffer_getreadvalidcount(session->send_buffer) > 0)
        {
            epollserver_send_olddata(session);
        }
    
        if(session->writeable && session->haveleftdata)
        {
            /*  如果socket仍然可写且逻辑层有未发送的数据则通知逻辑层    */
            struct server_s* server = &session->server->base;
            (server->logic_on_cansend)(server, session->index);
        }
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
        (*server->logic_on_close)(server, session->index);
    }
    else if(all_recvlen > 0)
    {
        int proc_len = (*server->logic_on_recved)(server, session->index, ox_buffer_getreadptr(recv_buffer), ox_buffer_getreadvalidcount(recv_buffer));
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
    logic_on_enter_pt enter_pt,
    logic_on_close_pt close_pt,
    logic_on_recved_pt   recved_pt,
    logic_on_cansend_pt    cansend_pt,
    logic_on_sendfinish_pt sendfinish_pt
    )
{
    struct epollserver_s* epollserver = (struct epollserver_s*)self;

    self->logic_on_enter = enter_pt;
    self->logic_on_close = close_pt;
    self->logic_on_recved = recved_pt;
    self->logic_on_cansend = cansend_pt;
    self->logic_on_sendfinish = sendfinish_pt;

    epollserver->freelist = ox_stack_new(epollserver->max_num, sizeof(struct session_s*));
    epollserver->freelist_num = epollserver->max_num;

    epollserver->sessions = (struct session_s*)malloc(sizeof(struct session_s)*epollserver->max_num);
    
    {
        int i = 0;
        for(; i < epollserver->max_num; ++i)
        {
            struct session_s* session = epollserver->sessions+i;
            session->server = epollserver;
            session->index = i;
            session->fd = SOCKET_ERROR;
            session->haveleftdata = false;
            
            session->send_buffer = ox_buffer_new(epollserver->session_sendbuffer_size);
            session->recv_buffer = ox_buffer_new(epollserver->session_recvbuffer_size);
            session->status = session_status_none;
            ox_stack_push(epollserver->freelist, &session);
        }
    }
    
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

    if(epollserver->sessions != NULL)
    {
        int i = 0;
        for(; i < epollserver->max_num; ++i)
        {
            struct session_s* session = epollserver->sessions+i;
            if(session->fd != SOCKET_ERROR)
            {
                ox_socket_close(session->fd);
                session->fd = SOCKET_ERROR;
            }

            if(session->send_buffer != NULL)
            {
                ox_buffer_delete(session->send_buffer);
                session->send_buffer = NULL;
            }

            if(session->recv_buffer != NULL)
            {
                ox_buffer_delete(session->recv_buffer);
                session->recv_buffer = NULL;
            }
        }

        free(epollserver->sessions);
        epollserver->sessions = NULL;
    }
    
    if(epollserver->freelist != NULL)
    {
        ox_stack_delete(epollserver->freelist);
        epollserver->freelist = NULL;
    }

    free(epollserver);
}

static void
epollserver_poll(struct server_s* self, int timeout)
{
    #ifdef PLATFORM_LINUX
    struct epollserver_s* epollserver = (struct epollserver_s*)self;
    int epollfd = epollserver->epoll_fd;
    struct epoll_event events[MAX_EVENTS];
    int current_time = ox_getnowtime();
    const int end_time = current_time + timeout;
    int ms = timeout;
    
    do
    {
        int i = 0;
        int nfds = epoll_wait(epollfd, events, MAX_EVENTS, ms);
        if(-1 == nfds)
        {
            if(S_EINTR == sErrno)
            {
                continue;
            }
            else
            {
                break;
            }
        }

        for(i = 0; i < nfds; ++i)
        {
            struct session_s*   session = (struct session_s*)(events[i].data.ptr);
            uint32_t event_data = events[i].events;
            
            if(event_data & EPOLLRDHUP)
            {
                /*  不能调用epoll_recvdata_callback实现处理客户端断开,因为数据和close可能连续到达,需要循环读取    */
                epollserver_halfclose_session(session->server, session);
                (*self->logic_on_close)(self, session->index);
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
        
        current_time = ox_getnowtime();
        ms = end_time-current_time;
    }while(ms > 0);

    #endif
}

static void
epollserver_closesession_callback(struct server_s* self, int index)
{
    struct epollserver_s* epollserver = (struct epollserver_s*)self;
    
    if(index >= 0 && index < epollserver->max_num)
    {
        struct session_s* session = epollserver->sessions+index;
        epollserver_handle_sessionclose(epollserver, session);
    }
}

static bool
epollserver_register(struct server_s* self, int fd)
{
    return epoll_handle_newclient((struct epollserver_s*)self, fd);
}

static int
epollserver_send_callback(struct server_s* self, int index, const char* data, int len)
{
    struct epollserver_s* epollserver = (struct epollserver_s*)self;
    int send_len = -1;

    if(index >= 0 && index < epollserver->max_num)
    {
        struct session_s* session = epollserver->sessions+index;
        send_len = epollserver_senddata(session, data, len);
    }
    
    return send_len;
}

static int
epollserver_sendv_callback(struct server_s* self, int index, const char* datas[], const int* lens, int num)
{
    int send_len = 0;
    struct epollserver_s* epollserver = (struct epollserver_s*)self;
    if(index >= 0 && index < epollserver->max_num && num > 0 && num <= MAX_SENDBUF_NUM)
    {
        struct session_s* session = epollserver->sessions+index;
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
    int max_num,
    int session_recvbuffer_size,
    int session_sendbuffer_size,
    void*   ext)
{
    struct epollserver_s* epollserver = (struct epollserver_s*)malloc(sizeof(*epollserver));
    memset(epollserver, 0, sizeof(*epollserver));

    epollserver->sessions = NULL;
    epollserver->max_num = max_num;

    epollserver->base.start_pt = epollserver_start_callback;
    epollserver->base.poll_pt = epollserver_poll;
    epollserver->base.stop_pt = epollserver_stop_callback;
    epollserver->base.closesession_pt = epollserver_closesession_callback;
    epollserver->base.register_pt = epollserver_register;
    epollserver->base.send_pt = epollserver_send_callback;
    epollserver->base.sendv_pt = epollserver_sendv_callback;
    /*  epollserver->base.copy_pt = epollserver_copy_callback; */
    epollserver->base.ext = ext;

    epollserver->session_recvbuffer_size = session_recvbuffer_size;
    epollserver->session_sendbuffer_size = session_sendbuffer_size;

    epollserver->freelist = NULL;
    epollserver->freelist_num = 0;
    epollserver->epoll_fd = SOCKET_ERROR;

    /*  去掉保护    signal(SIGPIPE, SIG_IGN);   */
    return &epollserver->base;
}
