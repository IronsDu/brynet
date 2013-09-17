#include <winsock2.h>
#include <WinSock.h>
#include <mswsock.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "socketlibfunction.h"
#include "server.h"
#include "server_private.h"
#include "thread.h"
#include "buffer.h"
#include "mutex.h"
#include "iocp.h"

// IO操作码
#define OVL_RECV    (1)
#define OVL_SEND    (2)
#define OVL_ACCEPT  (3)
#define OVL_RESET   (4)

// 完成键结构
struct complete_key_s
{
    void* ptr;
    bool fromlisten;
};

// 扩展重叠结构
struct ovl_ext_s 
{
    OVERLAPPED  base;
    void*   ptr;
};

struct session_s
{
    int index;
    sock fd;
    struct buffer_s*    send_buffer;
    struct buffer_s*    recv_buffer;
    struct mutex_s*  send_mutex;    // TODO::考虑换为自旋锁
    bool    send_ispending;
    char    ip[IP_SIZE];
    int port;
    bool active;

    void*   ext_data;
};

struct iocp_s
{
    struct server_s base;
    
    int port;
    bool run_flag;
    int work_threadnum;
    struct thread_s**  work_threads;
    HANDLE  iocp_handle;
    sock listenfd;
    struct complete_key_s listen_key;
    int max_num;
    struct session_s*   sessions;
    struct session_ext_s* exts;

    int close_num;
    struct mutex_s*  close_num_mutex;

    int session_recvbuffer_size;
    int session_sendbuffer_size;
};

#define ACCEPT_ADDRESS_LENGTH   (sizeof(SOCKADDR_IN) + 16)

//  struct session_s的扩展属性ext_data结构
struct session_ext_s
{
    struct complete_key_s ck;

    struct ovl_ext_s ovl_recv;
    struct ovl_ext_s ovl_send;
    struct ovl_ext_s ovl_accept_reset;
    char    addrblock[ACCEPT_ADDRESS_LENGTH*2];
};

static bool listen_port(struct iocp_s* iocp, int port)
{
    iocp->listenfd = WSASocket(PF_INET, SOCK_STREAM, IPPROTO_TCP, 0, 0, WSA_FLAG_OVERLAPPED);

    if(SOCKET_ERROR != iocp->listenfd)
    {
        SOCKADDR_IN Addr;
        ZeroMemory(&Addr, sizeof(Addr));
        Addr.sin_family = AF_INET;
        Addr.sin_addr.s_addr    = htonl(INADDR_ANY);
        Addr.sin_port   = htons(port);

        if(bind(iocp->listenfd, (struct sockaddr*)&Addr, sizeof(Addr)) < 0)
        {
            printf("bind port:%d failed\n", port);
            return false;
        }

        listen(iocp->listenfd, 2048);
        iocp->listen_key.fromlisten = true;
        iocp->listen_key.ptr = NULL;

        return NULL != CreateIoCompletionPort((HANDLE)iocp->listenfd, iocp->iocp_handle, (ULONG_PTR)&iocp->listen_key, 0);
    }

    return false;
}

static void iocp_session_reset(struct session_s* client)
{
    if(client->active)
    {
        struct session_ext_s*  ext_p = (struct session_ext_s*)client->ext_data;
        ext_p->ovl_accept_reset.base.Offset = OVL_RESET;
        client->active = false;

        // 重置套接字
        TransmitFile(client->fd, 0, 0, 0, (LPOVERLAPPED)&ext_p->ovl_accept_reset, 0, TF_DISCONNECT | TF_REUSE_SOCKET);
    }
}
static void iocp_session_onclose(struct iocp_s* iocp, struct session_s* client)
{
    if(client->active)
    {
        (iocp->base.logic_on_close)(&iocp->base, client->index);
    }
}

static void iocp_session_init(struct iocp_s* iocp, struct session_s* client)
{
    struct session_ext_s* ext_p = NULL;
    client->fd = WSASocket(PF_INET, SOCK_STREAM, IPPROTO_TCP, 0, 0, WSA_FLAG_OVERLAPPED);

    if(client->ext_data == NULL)
    {
        client->ext_data = (struct session_ext_s*)malloc(sizeof(struct session_ext_s));
    }

    ext_p = (struct session_ext_s*)client->ext_data;

    ext_p->ck.fromlisten = false;
    ext_p->ck.ptr = client;

    ZeroMemory(&ext_p->ovl_accept_reset, sizeof(struct ovl_ext_s));
    ZeroMemory(&ext_p->ovl_recv, sizeof(struct ovl_ext_s));
    ZeroMemory(&ext_p->ovl_send, sizeof(struct ovl_ext_s));

    ext_p->ovl_accept_reset.ptr = client;

    ext_p->ovl_accept_reset.base.Offset = OVL_ACCEPT;
    ext_p->ovl_recv.base.Offset = OVL_RECV;
    ext_p->ovl_send.base.Offset = OVL_SEND;
    
    client->recv_buffer = buffer_new(iocp->session_recvbuffer_size);
    client->send_buffer = buffer_new(iocp->session_sendbuffer_size);
    client->send_mutex = mutex_new();

    CreateIoCompletionPort((HANDLE)client->fd, iocp->iocp_handle, (DWORD)&ext_p->ck, 0);

    return;
}

static BOOL iocp_wait_accept(struct iocp_s* iocp, struct session_s* client)
{
    struct session_ext_s* ext_p = (struct session_ext_s*)client->ext_data;

    client->send_ispending = false;
    ext_p->ovl_accept_reset.base.Offset = OVL_ACCEPT;
    ZeroMemory(ext_p->addrblock, sizeof(ext_p->addrblock));

    return AcceptEx(iocp->listenfd, client->fd, ext_p->addrblock, 0, ACCEPT_ADDRESS_LENGTH, ACCEPT_ADDRESS_LENGTH, NULL, (LPOVERLAPPED)&ext_p->ovl_accept_reset);
}

static bool iocp_wait_recv(struct session_s* session, struct session_ext_s*  ext_p, sock fd)
{
    bool ret = true;
    
    if(session->active)
    {
        struct buffer_s* temp_read_buffer = session->recv_buffer;
        WSABUF  in_buf = {buffer_getwritevalidcount(temp_read_buffer), buffer_getwriteptr(temp_read_buffer)};

        if(in_buf.len > 0)
        {
            DWORD   flag = 0;
            DWORD   dwBytes = 0;
            int recv_ret = WSARecv(fd, &in_buf, 1, &dwBytes, &flag, &(ext_p->ovl_recv.base), 0);

            if(SOCKET_ERROR == recv_ret && sErrno != WSA_IO_PENDING)
            {
                ret = false;
            }
        }
    }

    return ret;
}

static bool iocp_recv_complete(struct iocp_s* iocp, struct session_s* session, struct session_ext_s* ext_p, int bytes)
{
    struct buffer_s* temp_buffer = session->recv_buffer;
    buffer_addwritepos(temp_buffer, bytes);

    {
        int process_len = (iocp->base.logic_on_recved)(&iocp->base, session->index, buffer_getreadptr(temp_buffer), buffer_getreadvalidcount(temp_buffer));
        buffer_addreadpos(temp_buffer, process_len);
    }

    // 如果接收缓冲区到了尾部,则调整缓冲区读写指针
    if(buffer_getwritepos(temp_buffer) == buffer_getsize(temp_buffer))
    {
        buffer_adjustto_head(temp_buffer);
    }

    return iocp_wait_recv(session, ext_p, session->fd);
}

static bool iocp_session_senddata(struct session_s* session, const char* data, int len)
{
    bool ret = (len <= 0);  // len <= 0 时返回值直接为true

    if(len > 0 && !session->send_ispending)
    {
        WSABUF  out_buf = {len, (char*)data};
        OVERLAPPED* ovl_p = &(((struct session_ext_s*)session->ext_data)->ovl_send.base);
        DWORD send_len = len;

        ovl_p->OffsetHigh   = len;

        if(SOCKET_ERROR == (WSASend(session->fd, &out_buf, 1, &send_len, 0, ovl_p, 0)))
        {
            if(sErrno == WSA_IO_PENDING)
            {
                session->send_ispending = true;
                ret = true;
            }
        }
        else
        {
            ret = true;
        }
    }

    return ret;
}

// 包装发送函数:先发送内置缓冲区没有发送的内容,然后发送指定缓冲区内容
static bool iocp_wrap_session_senddata(struct session_s* session, const char* data, int len)
{
    struct buffer_s* temp_buffer = session->send_buffer;
    bool ret = true;    // 默认为成功
    int temp_size = 0;

    mutex_lock(session->send_mutex);

    temp_size = buffer_getreadvalidcount(temp_buffer);

    // 先投递内置缓冲区未投递的数据
    ret = iocp_session_senddata(session, buffer_getreadptr(temp_buffer), temp_size);

    if(ret)
    {
        buffer_addreadpos(temp_buffer, temp_size);

        if(NULL != data)
        {
            ret = iocp_session_senddata(session, data, len);

            if(!ret)
            {
                // 如果投递失败则将预投递的数据拷贝至内置缓冲区
                ret = buffer_write(temp_buffer, data, len);
            }
        }
    }

    mutex_unlock(session->send_mutex);

    return ret;
}

static bool iocp_send_complete(struct iocp_s* iocp, struct session_s* session, struct session_ext_s* ext_p, int bytes)
{
    bool ret = true;

    mutex_lock(session->send_mutex);
    session->send_ispending = false;
    ret = iocp_wrap_session_senddata(session, NULL, 0);
    mutex_unlock(session->send_mutex);

    return  ret;
}

static void iocp_iocomplete(struct iocp_s* iocp, struct session_s* session, struct session_ext_s* ext_p, int offset, int bytes)
{
    bool must_close = false;

    if(offset == OVL_RECV)
    {
        must_close = !iocp_recv_complete(iocp, session, ext_p, bytes);
    }
    else if(offset == OVL_SEND)
    {
        must_close = !iocp_send_complete(iocp, session, ext_p, bytes);
    }

    if(must_close)
    {
        iocp_session_onclose(iocp, session);
    }
}

static void iocp_accept_complete(struct iocp_s* iocp, struct session_s* session)
{
    SOCKADDR_IN addr;
    int addr_len    = sizeof(addr);
    char* ip = NULL;

    setsockopt(session->fd, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char*)&iocp->listenfd, sizeof(iocp->listenfd));

    getpeername(session->fd, (struct sockaddr*)&addr, &addr_len);
    ip = inet_ntoa(addr.sin_addr);
    session->port = htons(addr.sin_port);
    CopyMemory(session->ip, ip, strlen(ip));

    buffer_init(session->recv_buffer);
    buffer_init(session->send_buffer);

    session->active = true;

    (iocp->base.logic_on_enter)(&iocp->base, session->index);

    if(!iocp_wait_recv(session, (struct session_ext_s*)session->ext_data, session->fd))
    {
        iocp_session_reset(session);
    }
}

static void iocp_work_thread(void* arg)
{
    struct ovl_ext_s*  ovl_p  = NULL;
    struct complete_key_s* ck_p   = NULL;
    struct session_s*    session_p    = NULL;
    struct session_ext_s* ext_s = NULL;
    DWORD Bytes   = 0;
    struct iocp_s* iocp = (struct iocp_s*)arg;

    for(;;)
    {
        bool Success = GetQueuedCompletionStatus(iocp->iocp_handle, &Bytes, (PULONG_PTR)&ck_p, (LPOVERLAPPED*)&ovl_p, INFINITE);

        if(NULL != ck_p)
        {
            if(ck_p->fromlisten)
            {
                session_p = (struct session_s*)ovl_p->ptr;
            }
            else
            {
                session_p = (struct session_s*)ck_p->ptr;
            }
        }
        else
        {
            if(Bytes == 0xDEADC0DE)
            {
                mutex_lock(iocp->close_num_mutex);
                ++iocp->close_num;
                mutex_unlock(iocp->close_num_mutex);
            }

            continue;
        }

        if(Success)
        {
            if((int)session_p == 0xFF)
            {
                printf("work thread exit\n");
                break;
            }
            else if(ck_p->fromlisten)
            {
                iocp_accept_complete(iocp, session_p);
            }
            else if(Bytes > 0)
            {
                ext_s = (struct session_ext_s*)session_p->ext_data;
                iocp_iocomplete(iocp, session_p, ext_s, ovl_p->base.Offset, Bytes);
            }
            else if(ovl_p->base.Offset == OVL_RESET)
            {
                iocp_wait_accept(iocp, session_p);
            }
            else if(Bytes == 0)
            {
                iocp_session_onclose(iocp, session_p);
                
            }
        }
        else
        {
            iocp_session_onclose(iocp, session_p);
        }
    }
}

static void iocp_startworkthreads(struct iocp_s* iocp)
{
    int i = 0;
    for (; i < iocp->work_threadnum; ++i)
    {
        iocp->work_threads[i] = thread_new(iocp_work_thread, iocp);
    }
}

static void iocp_start_callback(
    struct server_s* self,
    logic_on_enter_pt enter_pt,
    logic_on_close_pt close_pt,
    logic_on_recved_pt   recved_pt
    )
{
    struct iocp_s* iocp = (struct iocp_s*)self;
    socket_init();
    if(!iocp->run_flag)
    {
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        
        iocp->work_threadnum = 2 * si.dwNumberOfProcessors + 2;
        iocp->work_threads = (struct thread_s**)malloc(sizeof(struct thread_s*)*iocp->work_threadnum);
        iocp->iocp_handle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, iocp->work_threadnum);
        
        iocp->base.logic_on_enter = enter_pt;
        iocp->base.logic_on_close = close_pt;
        iocp->base.logic_on_recved = recved_pt;

        if (listen_port(iocp, iocp->port))
        {
            int i = 0;
            iocp->exts = (struct session_ext_s*)malloc(sizeof(struct session_ext_s) * iocp->max_num);
            iocp->sessions = (struct session_s*)malloc(sizeof(struct session_s) * iocp->max_num);
            
            memset(iocp->sessions, 0, sizeof(struct session_s) * iocp->max_num);
            iocp->close_num_mutex = mutex_new();

            for (; i < iocp->max_num; ++i)
            {
                iocp->sessions[i].ext_data = iocp->exts+i;
                iocp->sessions[i].index = i;
                iocp_session_init(iocp, iocp->sessions+i);
                iocp->sessions[i].active = false;
                iocp_wait_accept(iocp, iocp->sessions+i);
            }

            iocp_startworkthreads(iocp);

            iocp->close_num = 0;
            iocp->run_flag = true;
        }
    }
}

static void iocp_stop_callback(struct server_s* self)
{
    struct iocp_s* iocp = (struct iocp_s*)self;
    if(iocp->run_flag)
    {
        int i = 0;
        shutdown(iocp->listenfd, SD_BOTH);

        for(; i < iocp->max_num; ++i)
        {
            socket_close(iocp->sessions[i].fd);
            PostQueuedCompletionStatus(iocp->iocp_handle, 0xDEADC0DE, 0, 0);
        }

        while(iocp->max_num != iocp->close_num)
        {
            thread_sleep(1);
        }
        
        iocp->listen_key.fromlisten = false;
        iocp->listen_key.ptr= (void*)0xFF;

        i = 0;
        for (; i<iocp->work_threadnum; ++i)
        {
            PostQueuedCompletionStatus(iocp->iocp_handle, 0, (DWORD)&iocp->listen_key, 0);
        }

        if(iocp->work_threadnum > 0)
        {
            i = 0;
            for(; i < iocp->work_threadnum; ++i)
            {
                thread_delete(iocp->work_threads[i]);
            }
            
            free(iocp->work_threads);
            iocp->work_threadnum = 0;
            iocp->work_threads = NULL;
        }

        mutex_delete(iocp->close_num_mutex);
        iocp->close_num_mutex = NULL;

        socket_close(iocp->listenfd);
        iocp->listenfd = SOCKET_ERROR;

        i = 0;
        for(; i < iocp->max_num; ++i)
        {
            struct session_s* session = iocp->sessions+i;
            mutex_delete(session->send_mutex);
            buffer_delete(session->recv_buffer);
            buffer_delete(session->send_buffer);

            session->send_mutex = NULL;
            session->recv_buffer = NULL;
            session->send_buffer = NULL;
        }
        free(iocp->sessions);
        iocp->sessions = NULL;

        free(iocp->exts);
        iocp->exts = NULL;

        CloseHandle(iocp->iocp_handle);
        iocp->iocp_handle = NULL;
        iocp->run_flag = false;

        printf("退出\n");
    }
}

static int iocp_send_callback(struct server_s* self, int index, const char* data, int len)
{
    int send_len = 0;
    struct iocp_s* iocp = (struct iocp_s*)self;
    if(index >= 0 && index < iocp->max_num)
    {
        struct session_s* session = iocp->sessions+index;

        if(session->active)
        {
            if(iocp_wrap_session_senddata(session, data, len))
            {
                send_len = len;
            }
        }
    }

    return send_len;
}

static void iocp_closesession_callback(struct server_s* self, int index)
{
    struct iocp_s* iocp = (struct iocp_s*)self;
    iocp_session_reset(iocp->sessions+index);
}

struct server_s* iocp_create(
    int port, 
    int max_num,
    int session_recvbuffer_size,
    int session_sendbuffer_size)
{
    struct iocp_s* iocp = (struct iocp_s*)malloc(sizeof(struct iocp_s));
    memset(iocp, 0, sizeof(*iocp));

    iocp->base.start_pt = iocp_start_callback;
    iocp->base.stop_pt = iocp_stop_callback;
    iocp->base.closesession_pt = iocp_closesession_callback;
    iocp->base.send_pt = iocp_send_callback;

    iocp->port = port;
    iocp->run_flag = false;
    iocp->max_num = max_num;

    iocp->session_recvbuffer_size = session_recvbuffer_size;
    iocp->session_sendbuffer_size = session_sendbuffer_size;

    return &iocp->base;
}

void iocp_delete(struct server_s* self)
{
    iocp_stop_callback(self);
    free(self);
}
