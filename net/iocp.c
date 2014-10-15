#include <winsock2.h>
#include <WinSock.h>
#include <mswsock.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "socketlibfunction.h"
#include "server.h"
#include "server_private.h"
#include "buffer.h"
#include "mutex.h"
#include "stack.h"
#include "systemlib.h"
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
};

// 扩展重叠结构
struct ovl_ext_s 
{
    OVERLAPPED  base;
    void*   ptr;
};

struct session_ext_s;

struct session_s
{
    sock                fd;
    struct buffer_s*    recv_buffer;
    char                ip[IP_SIZE];
    int                 port;

    WSABUF              wsendbuf[MAX_SENDBUF_NUM];

    bool                send_ispending;     /*  发送是否挂起  */
    bool                active;             /*  连接是否正常    */
    bool                send_isuse;         /*  是否已经投递了send */

    void*                   ud;
    struct session_ext_s*   ext_data;
    bool                    post_close;
};

#define permormance 0

struct iocp_s
{
    struct server_s base;

    bool run_flag;
    HANDLE  iocp_handle;

    int session_recvbuffer_size;
    int session_sendbuffer_size;

#if permormance
    int send_count; /*  wsasend次数   */
    int total_recv_len; /*  总接收数据字节数    */
    int total_send_len; /*  总发送数据字节数    */

    int64_t old_time;
#endif
};

//  struct session_s的扩展属性ext_data结构
struct session_ext_s
{
    struct complete_key_s ck;

    struct ovl_ext_s ovl_recv;
    struct ovl_ext_s ovl_send;
};

static void iocp_session_reset(struct session_s* client)
{
    if(client->fd != SOCKET_ERROR)
    {
        ox_socket_close(client->fd);
        client->fd = SOCKET_ERROR;
        client->active = false;
        client->send_isuse = false;
    }
}

static void iocp_session_free(struct session_s* client)
{
    iocp_session_reset(client);
    ox_buffer_delete(client->recv_buffer);
    if(client->ext_data != NULL)
    {
        free(client->ext_data);
        client->ext_data = NULL;
    }
    free(client);
}

static struct session_s* iocp_session_malloc(struct iocp_s* iocp)
{
    struct session_s* session = (struct session_s*)malloc(sizeof *session);
    struct session_ext_s* ext_data = (struct session_ext_s*)malloc(sizeof *ext_data);

    if(session != NULL && ext_data != NULL)
    {
        session->ext_data = ext_data;

        ZeroMemory(&ext_data->ovl_recv, sizeof(struct ovl_ext_s));
        ZeroMemory(&ext_data->ovl_send, sizeof(struct ovl_ext_s));

        ext_data->ovl_recv.base.Offset = OVL_RECV;
        ext_data->ovl_send.base.Offset = OVL_SEND;

        ext_data->ck.ptr = session;

        session->recv_buffer = ox_buffer_new(iocp->session_recvbuffer_size);

        session->send_ispending = false;
        session->send_isuse = false;
        session->active = true;

        session->ud = NULL;
        session->fd = SOCKET_ERROR;
        session->post_close = false;
    }
    else
    {
        if(session != NULL)
        {
            free(session);
            session = NULL;
        }

        if(ext_data != NULL)
        {
            free(ext_data);
            ext_data = NULL;
        }
    }
    
    return session;
}

static bool iocp_wait_recv(struct session_s* session, struct session_ext_s*  ext_p, sock fd)
{
    bool ret = true;
    
    if(session->active)
    {
        struct buffer_s* temp_read_buffer = session->recv_buffer;
        WSABUF  in_buf = {ox_buffer_getwritevalidcount(temp_read_buffer), ox_buffer_getwriteptr(temp_read_buffer)};

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
        else
        {
            assert(false);
        }
    }

    return ret;
}

static bool iocp_recv_complete(struct iocp_s* iocp, struct session_s* session, struct session_ext_s* ext_p, int bytes)
{
    struct buffer_s* temp_buffer = session->recv_buffer;
    ox_buffer_addwritepos(temp_buffer, bytes);

    {
        int process_len = (iocp->base.logic_on_recved)(&iocp->base, session->ud, ox_buffer_getreadptr(temp_buffer), ox_buffer_getreadvalidcount(temp_buffer));
        ox_buffer_addreadpos(temp_buffer, process_len);
    }

    // 如果接收缓冲区到了尾部,则调整缓冲区读写指针
    if(ox_buffer_getwritepos(temp_buffer) == ox_buffer_getsize(temp_buffer))
    {
        ox_buffer_adjustto_head(temp_buffer);
    }

    return iocp_wait_recv(session, ext_p, session->fd);
}

static void iocp_send_complete(struct iocp_s* iocp, struct session_s* session, struct session_ext_s* ext_p, int bytes)
{
    session->send_ispending = false;
    session->send_isuse = false;

    if(iocp->base.logic_on_sendfinish)
    {
        /*  调用上层发送完成通知  */
        /*  用于上层采用sendv接口时,调用此函数直接路由给上层进行下一步处理,此处不做任何其他操作   */
        (iocp->base.logic_on_sendfinish)(&iocp->base, session->ud, bytes);
    }
}

static void iocp_session_on_disconnect(struct iocp_s* iocp, struct session_s* client)
{
    if (client->active)
    {
        /*  如果当前会话处于活动，则通知上层  */
        client->active = false;
        (iocp->base.logic_on_disconnection)(&iocp->base, client->ud);
    }
}

static void iocp_iocomplete(struct iocp_s* iocp, struct session_s* session, struct session_ext_s* ext_p, int offset, int bytes)
{
    bool must_close = false;

    if(offset == OVL_RECV)
    {
#if permormance
        if(bytes > 0)
        {
            iocp->total_recv_len += bytes;
        }
#endif
        must_close = !iocp_recv_complete(iocp, session, ext_p, bytes);
    }
    else if(offset == OVL_SEND)
    {
#if permormance
        if(bytes > 0)
        {
            iocp->total_send_len += bytes;
        }
#endif
        iocp_send_complete(iocp, session, ext_p, bytes);
    }

    if(must_close)
    {
        iocp_session_on_disconnect(iocp, session);
    }
}

static bool iocp_accept_complete(struct iocp_s* iocp, struct session_s* session)
{
    SOCKADDR_IN addr;
    int addr_len    = sizeof(addr);
    char* ip = NULL;
    struct session_ext_s* ext = (struct session_ext_s*)session->ext_data;

    getpeername(session->fd, (struct sockaddr*)&addr, &addr_len);
    ip = inet_ntoa(addr.sin_addr);
    session->port = htons(addr.sin_port);
    strcpy_s(session->ip, sizeof(session->ip), ip);

    ox_socket_nonblock(session->fd);

    CreateIoCompletionPort((HANDLE)session->fd, iocp->iocp_handle, (DWORD)&(ext->ck), 0);

    if(!iocp_wait_recv(session, ext, session->fd))
    {
        iocp_session_reset(session);
        return false;
    }

    return true;
}

static void
iocp_poll_callback(struct server_s* self, int64_t timeout)
{
    struct iocp_s* iocp = (struct iocp_s*)self;
    struct ovl_ext_s*  ovl_p  = NULL;
    struct complete_key_s* ck_p   = NULL;
    struct session_s*    session_p    = NULL;
    struct session_ext_s* ext_s = NULL;
    DWORD Bytes   = 0;
    int64_t current_time = ox_getnowtime();
    int64_t end_time = current_time + timeout;
    int64_t ms = end_time - current_time;

    do
    {
        bool Success = GetQueuedCompletionStatus(iocp->iocp_handle, &Bytes, (PULONG_PTR)&ck_p, (LPOVERLAPPED*)&ovl_p, ms);

        if(!Success && ovl_p == NULL)    break;

        session_p = (struct session_s*)ck_p->ptr;

        if(Bytes == 0)
        {
            if(ovl_p == (struct ovl_ext_s*)0xcdcdcdcd)
            {
                /*  完成最后释放资源，并通知上层（才能释放其资源）  */
                void* ud = session_p->ud;
                iocp_session_free(session_p);
                (self->logic_on_closecompleted)(self, ud);
            }
            else
            {
                /*  处理被动关闭socket    */
                iocp_session_on_disconnect(iocp, session_p);
            }
        }
        else
        {
            ext_s = (struct session_ext_s*)session_p->ext_data;
            iocp_iocomplete(iocp, session_p, ext_s, ovl_p->base.Offset, Bytes);
        }

        current_time = ox_getnowtime();
        ms = end_time - current_time;
        ovl_p = NULL;
    }while(ms > 0);

#if permormance
    {
        int64_t now_time = 0;
        if(iocp->old_time == 0)
        {
            iocp->old_time = ox_getnowtime();
        }

        now_time = ox_getnowtime();
        if((now_time - iocp->old_time) >= 1000)
        {
            iocp->old_time = now_time;
            printf("%p limit[%d] send count:%d /s, recv %dK/s, send %dK/s\n", iocp, iocp->max_num, iocp->send_count, iocp->total_recv_len/1024, iocp->total_send_len/1024);
            iocp->send_count = 0;
            iocp->total_recv_len = 0;
            iocp->total_send_len = 0;
        }
    }
#endif
}

static void iocp_start_callback(
    struct server_s* self,
    logic_on_enter_handle enter_pt,
    logic_on_disconnection_handle disconnection_pt,
    logic_on_recved_handle   recved_pt,
    logic_on_sendfinish_handle  sendfinish_pt,
    logic_on_close_completed    closecompleted_pt
    )
{
    struct iocp_s* iocp = (struct iocp_s*)self;
    ox_socket_init();
    if(!iocp->run_flag)
    {
        SYSTEM_INFO si;
        int i = 0;
        GetSystemInfo(&si);

        iocp->iocp_handle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 1);
        
        iocp->base.logic_on_enter = enter_pt;
        iocp->base.logic_on_disconnection = disconnection_pt;
        iocp->base.logic_on_recved = recved_pt;
        iocp->base.logic_on_sendfinish = sendfinish_pt;
        iocp->base.logic_on_closecompleted = closecompleted_pt;

        iocp->run_flag = true;
    }
}

static void iocp_stop_callback(struct server_s* self)
{
    struct iocp_s* iocp = (struct iocp_s*)self;
    if(iocp->run_flag)
    {
        CloseHandle(iocp->iocp_handle);
        iocp->iocp_handle = NULL;
        iocp->run_flag = false;

        free(iocp);
    }
}

/*  返回-1表示socket断开,否则返回0  */
static int iocp_sendv_callback(struct server_s* self, void* handle, const char* datas[], const int* lens, int num)
{
    /*  同一时刻只能存在一次wsasend   */
    int send_len = 0;
    struct iocp_s* iocp = (struct iocp_s*)self;
    if(num > 0 && num <= MAX_SENDBUF_NUM)
    {
        struct session_s* session = (struct session_s*)handle;

        if(session->active && !session->send_isuse && !session->send_ispending)
        {
            int     i = 0;
            int     totalsendlen = 0;
            OVERLAPPED* ovl_p = NULL;

            for(; i < MAX_SENDBUF_NUM && i < num; ++i)
            {
                session->wsendbuf[i].buf = (CHAR*)datas[i];
                session->wsendbuf[i].len = lens[i];
                totalsendlen += lens[i];
            }

            ovl_p = &(((struct session_ext_s*)session->ext_data)->ovl_send.base);

            ovl_p->OffsetHigh   = totalsendlen;

            /*  windows下sendv接口如果发送成功则返回0   */
            if(SOCKET_ERROR == WSASend(session->fd, &session->wsendbuf[0], i, (LPDWORD)&send_len, 0, ovl_p, 0))
            {
                if(sErrno == WSA_IO_PENDING)
                {
                    session->send_ispending = true;
                    send_len = 0;
                }
                else
                {
                    send_len = -1;
                    perror("The following error occurred");
                }
            }
            else
            {
                send_len = 0;
            }

#if permormance
            iocp->send_count ++;
#endif
            session->send_isuse = true;
        }
    }

    return send_len;
}

static void iocp_closesession_callback(struct server_s* self, void* handle)
{
    /*  仅投递释放资源的请求  */

    struct iocp_s* iocp = (struct iocp_s*)self;
    struct session_s* session = (struct session_s*)handle;
    struct session_ext_s* ext_data = (struct session_ext_s*)session->ext_data;

    iocp_session_reset(session);

    if(!session->post_close)
    {
        /*  投递特殊通知，以做最后的释放动作    */
        session->post_close = true;
        PostQueuedCompletionStatus(iocp->iocp_handle, 0, (ULONG_PTR)&(ext_data->ck), (LPOVERLAPPED)0xcdcdcdcd);
    }
}

static bool iocp_register_callback(struct server_s* self, void* ud, int fd)
{
    bool ret = false;
    struct iocp_s* iocp = (struct iocp_s*)self;
    struct session_s* session = iocp_session_malloc(iocp);
    session->fd = fd;
    session->ud = ud;

    if(iocp_accept_complete(iocp, session))
    {
        (iocp->base.logic_on_enter)(&iocp->base, session->ud, session);

        ret = true;
    }
    else
    {
        iocp_session_free(session);
    }

    return ret;
}

struct server_s* iocp_create(
    int session_recvbuffer_size,
    int session_sendbuffer_size,
    void*   ext)
{
    struct server_s* ret = NULL;
    struct iocp_s* iocp = (struct iocp_s*)malloc(sizeof(struct iocp_s));
    if(iocp != NULL)
    {
        memset(iocp, 0, sizeof(*iocp));

        iocp->base.start_callback = iocp_start_callback;
        iocp->base.poll_callback = iocp_poll_callback;
        iocp->base.stop_callback = iocp_stop_callback;
        iocp->base.closesession_callback = iocp_closesession_callback;
        iocp->base.register_callback = iocp_register_callback;
        iocp->base.sendv_callback = iocp_sendv_callback;

        iocp->base.ext = ext;
        iocp->run_flag = false;

        iocp->session_recvbuffer_size = session_recvbuffer_size;
        iocp->session_sendbuffer_size = session_sendbuffer_size;

#if permormance
        iocp->send_count = 0;
        iocp->total_recv_len = 0;
        iocp->total_send_len = 0;
        iocp->old_time = 0;
#endif
        ret = &iocp->base;
    }

    return ret;
}

void iocp_delete(struct server_s* self)
{
    iocp_stop_callback(self);
    free(self);
}
