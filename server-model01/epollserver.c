#include "platform.h"

#ifdef PLATFORM_LINUX
#include <sys/epoll.h>
#include <signal.h>
#ifndef EPOLLRDHUP
    #define EPOLLRDHUP (0x2000)
    #endif
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "stack.h"
#include "mutex.h"
#include "socketlibtypes.h"
#include "socketlibfunction.h"
#include "thread.h"
#include "threadpool.h"
#include "server_private.h"
#include "buffer.h"

#define EPOLL_WORKTHREAD_NUM (2)    /*  epoll_wait线程个数; recv线程池的工作线程个数  */

#define MAX_EVENTS (200)

enum EPOLL_NET_USE_SPINLOCK
{
    EPOLL_NET_USE_SPINLOCK_OPEN = 1,
};

struct epollserver_s;
struct epollserver_reactor_s;

struct session_s
{
    struct epollserver_reactor_s*   reactor;
    int index;
    sock fd;

    bool    writeable;               /*  是否可写    */
    
    struct mutex_s*  send_mutex;
    struct thread_spinlock_s* send_spinlock;
    
    struct buffer_s*    send_buffer;
    struct mutex_s* recv_mutex;
    struct buffer_s*    recv_buffer;
};

struct epollserver_reactor_s
{
    struct epollserver_s* master;
    int epoll_fd;
    int fd_num;
};

struct epollserver_s
{
    struct server_s base;
    int listen_port;

    int session_recvbuffer_size;
    int session_sendbuffer_size;

    int max_num;
    struct session_s*   sessions;

    struct stack_s* freelist;
    int freelist_num;
    
    struct mutex_s* freelist_mutex;
    struct thread_spinlock_s* freelist_spinlock;

    struct thread_s*    listen_thread;
    
    int reactor_num;
    struct epollserver_reactor_s* reactors;
    struct thread_s**  epoll_threads;           /*  epoll_wait线程组   */
    struct thread_pool_s*   recv_thread_pool;   /*  recv线程池(处理epoll_wait的可读事件) */
};

#ifdef PLATFORM_LINUX
static void 
epoll_add_event(struct epollserver_reactor_s* reactor, sock fd, struct session_s* client, uint32_t events)
{
    struct epoll_event ev = { 0, { 0 }};
    ev.events = events;

    ev.data.fd = fd;
    ev.data.ptr = client;
    epoll_ctl(reactor->epoll_fd, EPOLL_CTL_ADD, fd, &ev);
}
#endif

static void 
epoll_freelist_lock(struct epollserver_s* epollserver)
{
    if(EPOLL_NET_USE_SPINLOCK_OPEN)
    {
        thread_spinlock_lock(epollserver->freelist_spinlock);
    }
    else
    {
        mutex_lock(epollserver->freelist_mutex);
    }
}

static void 
epoll_freelist_unlock(struct epollserver_s* epollserver)
{
    if(EPOLL_NET_USE_SPINLOCK_OPEN)
    {
        thread_spinlock_unlock(epollserver->freelist_spinlock);
    }
    else
    {
        mutex_unlock(epollserver->freelist_mutex);
    }
}

static void 
epoll_handle_newclient(struct epollserver_s* epollserver, sock client_fd)
{
    if(epollserver->freelist_num > 0)
    {
        struct session_s*   session = NULL;

        epoll_freelist_lock(epollserver);

        {
            struct session_s** ppsession = (struct session_s**)stack_popback(epollserver->freelist);
            if(ppsession != NULL)
            {
                session = *ppsession;
                epollserver->freelist_num--;
            }
        }

        epoll_freelist_unlock(epollserver);

        if(session != NULL)
        {
            struct server_s* server = &epollserver->base;
            
            /*  TODO::按照某种算法得到当前最轻松的reactor */
            
            struct epollserver_reactor_s* reactor = epollserver->reactors+0;
            
            {
                int i = 1;
                for(; i < epollserver->reactor_num; ++i)
                {
                    struct epollserver_reactor_s* temp = epollserver->reactors+i;
                    if(temp->fd_num < reactor->fd_num)
                    {
                        reactor = temp;
                    }
                }
            }
            
            
            session->fd = client_fd;
            buffer_init(session->recv_buffer);
            buffer_init(session->send_buffer);
            socket_nonblock(client_fd);
            session->writeable = true;
            session->reactor = reactor;
            reactor->fd_num++;

            (*(server->logic_on_enter))(server, session->index);
            
            #ifdef PLATFORM_LINUX
            epoll_add_event(reactor, client_fd, session, EPOLLET | EPOLLIN | EPOLLOUT | EPOLLRDHUP);
            #endif
        }
        else
        {
            printf("没有可用资源\n");
            getchar();
        }
    }
    else
    {
        printf("没有可用资源\n");
        getchar();
    }
}

static void 
epollserver_handle_sessionclose(struct session_s* session)
{
    struct epollserver_reactor_s* reactor = session->reactor;
    struct epollserver_s* epollserver = reactor->master;
    
    epoll_freelist_lock(epollserver);

    if(SOCKET_ERROR != session->fd)
    {
#ifdef PLATFORM_LINUX
        epoll_ctl(reactor->epoll_fd, EPOLL_CTL_DEL, session->fd, NULL);
#endif
        socket_close(session->fd);
        session->fd = SOCKET_ERROR; 
        stack_push(epollserver->freelist, &session);
        epollserver->freelist_num++;
        reactor->fd_num--;
    }

    epoll_freelist_unlock(epollserver);

}

/*  TODO::考虑是否根据当前剩余session的个数来进行accept   */
static void 
epoll_listen_thread(void* arg)
{
    struct epollserver_s* epollserver = (struct epollserver_s*)arg;
    
    sock client_fd = SOCKET_ERROR;
    struct sockaddr_in socketaddress;
    socklen_t size = sizeof(struct sockaddr);

    sock listen_fd = socket_listen(epollserver->listen_port, 25);
    int total_num = 0;
    
    if(SOCKET_ERROR != listen_fd)
    {
        for(;;)
        {
            while((client_fd = accept(listen_fd, (struct sockaddr*)&socketaddress, &size)) < 0)
            {
                if(EINTR == sErrno)
                {
                    continue;
                }
            }

            if(SOCKET_ERROR != client_fd)
            {
                total_num++;
                printf("accept %d, current total num : %d\n", client_fd, total_num);
                epoll_handle_newclient(epollserver, client_fd);
            }
        }
        
        socket_close(listen_fd);
        listen_fd = SOCKET_ERROR;
    }
    else
    {
        printf("listen failed\n");
    }
}

/*  发送内置缓冲区未发送的数据 */
static void 
epollserver_send_olddata(struct session_s* session)
{
    int send_len = 0;
    struct buffer_s*    send_buffer = session->send_buffer;
    int oldlen = buffer_getreadvalidcount(send_buffer);
        
    if(oldlen > 0)
    {
        send_len = socket_send(session->fd, buffer_getreadptr(send_buffer), oldlen);

        if(send_len > 0)
        {
            buffer_addwritepos(send_buffer, send_len);
        }
        
        session->writeable = (oldlen == oldlen);
    }
    
    return;
}

static void 
session_send_lock(struct session_s* session)
{
    if(EPOLL_NET_USE_SPINLOCK_OPEN)
    {
        thread_spinlock_lock(session->send_spinlock);
    }
    else
    {
        mutex_lock(session->send_mutex);
    }
}

static void 
session_send_unlock(struct session_s* session)
{
    if(EPOLL_NET_USE_SPINLOCK_OPEN)
    {
        thread_spinlock_unlock(session->send_spinlock);
    }
    else
    {
        mutex_unlock(session->send_mutex);
    }
}

/*  优先发送内置缓冲区未发送的数据,然后发送指定数据 */
static int 
epollserver_senddata(struct session_s* session, const char* data, int len)
{
    int send_len = 0;
    struct buffer_s*    send_buffer = session->send_buffer;
    bool writeable = session->writeable;
    
    if(data == NULL || len == 0)
    {
        return 0;
    }
    
    session_send_lock(session);
    
    if(writeable)    /*  如果可写则发送内置缓冲区数据,然后仍然可写则继续发送指定数据  */
    {
        epollserver_send_olddata(session);
        
        if(session->writeable)
        {
            send_len = socket_send(session->fd, data, len);
        }
        
        if(send_len < len)
        {
            session->writeable = false;  /*  设置为不可写  */
        }
    }
    
    if(send_len < len)  /*  如果发送到socket的长度小于预发送的长度，则copy到session内置缓冲区  */
    {
        if(buffer_write(send_buffer, data+send_len, len-send_len))
        {
            send_len = len;
        }
    }

    session_send_unlock(session);
    
    return send_len;
}

static void 
epoll_handle_onoutevent(struct session_s* session)
{
    session->writeable = true;
    
    if(buffer_getreadvalidcount(session->send_buffer) > 0)
    {
        session_send_lock(session);
        
        epollserver_send_olddata(session);
        
        session_send_unlock(session);
    }
}

static void epoll_recvdata_callback(struct thread_pool_s* self, void* msg);

static void 
epoll_work_thread(void* arg)
{
    #ifdef PLATFORM_LINUX

    struct epollserver_reactor_s* reactor = (struct epollserver_reactor_s*)arg;
    int nfds = 0;
    int i = 0;
    int epollfd = reactor->epoll_fd;
    struct epoll_event events[MAX_EVENTS];
    uint32_t event_data = 0;

    for(;;)
    {
        nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
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
            event_data = events[i].events;
            
            if(event_data & EPOLLRDHUP)
            {
                //thread_pool_pushmsg(reactor->master->recv_thread_pool, session);
                //continue;
            }
            
            if(event_data & EPOLLIN)
            {
                /*  当此线程获得fd的可读事件后投递,然而此事件未处理之前,另外的线程又获取了fd的新可读通知
                 *  那么线程池的消息队列里将会有fd的多个可读消息
                 *  TODO::可以考虑优化    */
                /*  TODO::下面这种方式还不完全正确，某些情况客户端频繁链接，关闭。会导致服务器无法获取链接断开的消息 */
                /*  thread_pool_pushmsg(epollserver->recv_thread_pool, session);    */
                
                /*
                TODO::可以直接recv数据而非发送事件到线程池
                * */
                epoll_recvdata_callback(NULL, session);
            }

            if(event_data & EPOLLOUT)
            {
                epoll_handle_onoutevent(session);
            }
        }
    }

    #endif
}

/*  多线程epoll_wait时会在多个线程触发epollin事件,从来recv处理函数会存在线程安全问题,故加锁处理 */
/*  TODO::对于锁的加锁位置设计需要仔细考虑  */
/*  如果每个FD都只在一个epollfd中wait,则无需加锁。但即便加锁影响几乎没有，因为不存在竞争。  */
static void 
epoll_recvdata_callback(struct thread_pool_s* self, void* msg)
{
    struct session_s*   session = (struct session_s*)msg;
    struct buffer_s*    recv_buffer = session->recv_buffer;
    /*  TODO::优化    */
    struct server_s*    server = &(session->reactor->master->base);
    int all_recvlen = 0;
    bool is_close = false;
    
    bool proc_begin = false;
    
    if(session->fd == SOCKET_ERROR)
    {
        return;
    }
    
    mutex_lock(session->recv_mutex);

procbegin:
    proc_begin = false;
    all_recvlen = 0;
    
    for(;;)
    {
        int can_recvlen = 0;
        int recv_len = 0;

        if(buffer_getwritevalidcount(recv_buffer) <= 0)
        {
            buffer_adjustto_head(recv_buffer);
        }

        can_recvlen = buffer_getwritevalidcount(recv_buffer);

        if(can_recvlen <= 0)
        {
            /*
            TODO::设置再处理标志(逻辑层处理之后继续recv),可以处理客户端发送大量数据导致服务器
             有接收完客户端之前发送的数据,从而客户端后面预发送的数据一直发送不成功
             * */
            proc_begin = true;
            break;
        }

        recv_len = recv(session->fd, buffer_getwriteptr(recv_buffer), can_recvlen, 0);
        if(0 == recv_len)
        {
            is_close = true;
            break;
        }
        else if(SOCKET_ERROR == recv_len)
        {
            is_close = (S_EWOULDBLOCK != sErrno);
            /*  当客户端多次send时,EPOLL对同一个FD返回会多次EPOLLIN，然而第一个消息被处理时全部读入数据
             *  然后后面的消息被处理时则不能读入数据,于是返回失败
             *  TODO::见thread_pool_pushmsg的注释
             */
            break;
        }
        else
        {
            all_recvlen += recv_len;
            buffer_addwritepos(recv_buffer, recv_len);
        }
    }

    if(is_close)
    {
        /*
        TODO::有了proc_begin设计的情况下,或许得考虑在logic_on_recved中逻辑层已经调用了closesession接口
        而现在又回调逻辑层的close处理函数,那么逻辑层必须得设置标志处理这种情况
        以及网络层closesession和逻辑层closesession同时发生的情况
        * 
        * TODO::那么recv返回的错误值是多少呢？
        * */
        (*server->logic_on_close)(server, session->index);
    }
    else if(all_recvlen > 0)
    {
        int proc_len = (*server->logic_on_recved)(server, session->index, buffer_getreadptr(recv_buffer), buffer_getreadvalidcount(recv_buffer));
        buffer_addreadpos(recv_buffer, proc_len);
    }
    
    if(proc_begin && session->fd != SOCKET_ERROR)   /*  确保逻辑层在logic_on_recved中没有调用关闭session */
    {
        goto procbegin;
    }

    mutex_unlock(session->recv_mutex);
}

static void 
epollserver_start_callback(
    struct server_s* self,
    logic_on_enter_pt enter_pt,
    logic_on_close_pt close_pt,
    logic_on_recved_pt   recved_pt
    )
{
    struct epollserver_s* epollserver = (struct epollserver_s*)self;

    self->logic_on_enter = enter_pt;
    self->logic_on_close = close_pt;
    self->logic_on_recved = recved_pt;

    epollserver->freelist = stack_new(epollserver->max_num, sizeof(struct session_s*));
    epollserver->freelist_num = epollserver->max_num;
    
    if(EPOLL_NET_USE_SPINLOCK_OPEN)
    {
        epollserver->freelist_spinlock = thread_spinlock_new();
    }
    else
    {
        epollserver->freelist_mutex = mutex_new();
    }

    epollserver->sessions = (struct session_s*)malloc(sizeof(struct session_s)*epollserver->max_num);
    
    {
        int i = 0;
        for(; i < epollserver->max_num; ++i)
        {
            struct session_s* session = epollserver->sessions+i;
            session->index = i;
            session->reactor = NULL;
            session->fd = SOCKET_ERROR;

            if(EPOLL_NET_USE_SPINLOCK_OPEN)
            {
                session->send_spinlock = thread_spinlock_new();
            }
            else
            {
                session->send_mutex = mutex_new();
            }
            
            session->send_buffer = buffer_new(epollserver->session_sendbuffer_size);
            session->recv_mutex = mutex_new();
            session->recv_buffer = buffer_new(epollserver->session_recvbuffer_size);

            stack_push(epollserver->freelist, &session);
        }
    }

    epollserver->reactor_num = EPOLL_WORKTHREAD_NUM;
    
    epollserver->reactors = (struct epollserver_reactor_s*)malloc(sizeof(struct epollserver_reactor_s)*epollserver->reactor_num);
    
    {
        int i = 0;
        for(; i < epollserver->reactor_num; ++i)
        {
            #ifdef PLATFORM_LINUX
            epollserver->reactors[i].epoll_fd = epoll_create(1);
            epollserver->reactors[i].master = epollserver;
            epollserver->reactors[i].fd_num = 0;
            #endif
        }
    }
    
    /*  开启epoll_wait线程组 */
    epollserver->epoll_threads = (struct thread_s**)malloc(sizeof(struct thread_s*)*epollserver->reactor_num);

    {
        int i = 0;
        for(; i < epollserver->reactor_num; ++i)
        {
            epollserver->epoll_threads[i] = thread_new(epoll_work_thread, epollserver->reactors+i);
        }
    }

    /*  TODO::线程池的消息队列上限需要考虑拿给客户指定,如果太小则会导致事件处理不了  */
    /*  开启recv线程池   */
    epollserver->recv_thread_pool = thread_pool_new(epoll_recvdata_callback, epollserver->reactor_num, 102400);
    thread_pool_start(epollserver->recv_thread_pool);

    /*  开启监听线程  */
    epollserver->listen_thread = thread_new(epoll_listen_thread, epollserver);
}

static void 
epollserver_stop_callback(struct server_s* self)
{
}

static void 
epollserver_closesession_callback(struct server_s* self, int index)
{
    struct epollserver_s* epollserver = (struct epollserver_s*)self;
    
    if(index >= 0 && index < epollserver->max_num)
    {
        struct session_s* session = epollserver->sessions+index;
        epollserver_handle_sessionclose(session);
    }
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

struct server_s* 
epollserver_create(
    int port, 
    int max_num,
    int session_recvbuffer_size,
    int session_sendbuffer_size)
{
    struct epollserver_s* epollserver = (struct epollserver_s*)malloc(sizeof(*epollserver));
    memset(epollserver, 0, sizeof(*epollserver));

    epollserver->sessions = NULL;
    epollserver->max_num = max_num;
    epollserver->listen_port = port;

    epollserver->base.start_pt = epollserver_start_callback;
    epollserver->base.stop_pt = epollserver_stop_callback;
    epollserver->base.closesession_pt = epollserver_closesession_callback;
    epollserver->base.send_pt = epollserver_send_callback;

    epollserver->session_recvbuffer_size = session_recvbuffer_size;
    epollserver->session_sendbuffer_size = session_sendbuffer_size;

    signal(SIGPIPE, SIG_IGN);
    return &epollserver->base;
}
