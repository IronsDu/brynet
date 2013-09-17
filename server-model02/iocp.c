#include <winsock2.h>
#include <WinSock.h>
#include <mswsock.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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

struct session_s
{
    int index;
    sock fd;
    struct buffer_s*    send_buffer;
    struct buffer_s*    recv_buffer;
    bool    send_ispending;
    char    ip[IP_SIZE];
    int port;
    bool active;            /*  是否连接    */

    WSABUF wsendbuf[MAX_SENDBUF_NUM];

    bool    is_use;
    bool    haveleftdata;   /*  逻辑层没有发送完数据标志    */
    void*   ext_data;
};

#define permormance 0

struct iocp_s
{
    struct server_s base;

    bool run_flag;
    HANDLE  iocp_handle;
    int max_num;
    struct session_s*   sessions;
    struct session_ext_s* exts;

    int session_recvbuffer_size;
    int session_sendbuffer_size;

    struct stack_s* freelist;
    int freelist_num;

#if permormance
    int send_count; /*  wsasend次数   */
    int total_recv_len; /*  总接收数据字节数    */
    int total_send_len; /*  总发送数据字节数    */

    int64_t old_time;
#endif
};

#define ACCEPT_ADDRESS_LENGTH   (sizeof(SOCKADDR_IN) + 16)

//  struct session_s的扩展属性ext_data结构
struct session_ext_s
{
    struct complete_key_s ck;

    struct ovl_ext_s ovl_recv;
    struct ovl_ext_s ovl_send;
    char    addrblock[ACCEPT_ADDRESS_LENGTH*2];
};

static void iocp_session_reset(struct session_s* client)
{
    if(client->fd != SOCKET_ERROR)
    {
        struct session_ext_s*  ext_p = (struct session_ext_s*)client->ext_data;
        client->active = false;
        ox_socket_close(client->fd);
        client->fd = SOCKET_ERROR;
        client->is_use = false;
    }
}

static void iocp_session_onclose(struct iocp_s* iocp, struct session_s* client)
{
    if(client->active)
    {
        client->active = false;
        (iocp->base.logic_on_close)(&iocp->base, client->index);
    }
}

static void iocp_session_init(struct iocp_s* iocp, struct session_s* client)
{
    struct session_ext_s* ext_p = NULL;

    if(client->ext_data == NULL)
    {
        client->ext_data = (struct session_ext_s*)malloc(sizeof(struct session_ext_s));
    }

    ext_p = (struct session_ext_s*)client->ext_data;

    ZeroMemory(&ext_p->ovl_recv, sizeof(struct ovl_ext_s));
    ZeroMemory(&ext_p->ovl_send, sizeof(struct ovl_ext_s));

    ext_p->ovl_recv.base.Offset = OVL_RECV;
    ext_p->ovl_send.base.Offset = OVL_SEND;

    ext_p->ck.ptr = client;
    
    client->recv_buffer = ox_buffer_new(iocp->session_recvbuffer_size);
    client->send_buffer = ox_buffer_new(iocp->session_sendbuffer_size);

    client->haveleftdata = false;
    client->fd = SOCKET_ERROR;

    return;
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
    }

    return ret;
}

static bool iocp_recv_complete(struct iocp_s* iocp, struct session_s* session, struct session_ext_s* ext_p, int bytes)
{
    struct buffer_s* temp_buffer = session->recv_buffer;
    ox_buffer_addwritepos(temp_buffer, bytes);

    {
        int process_len = (iocp->base.logic_on_recved)(&iocp->base, session->index, ox_buffer_getreadptr(temp_buffer), ox_buffer_getreadvalidcount(temp_buffer));
        ox_buffer_addreadpos(temp_buffer, process_len);
    }

    // 如果接收缓冲区到了尾部,则调整缓冲区读写指针
    if(ox_buffer_getwritepos(temp_buffer) == ox_buffer_getsize(temp_buffer))
    {
        ox_buffer_adjustto_head(temp_buffer);
    }

    return iocp_wait_recv(session, ext_p, session->fd);
}

static int iocp_session_senddata(struct session_s* session, const char* data, int len)
{
    int send_len = 0;

    if(len > 0 && !session->send_ispending)
    {
        WSABUF  out_buf = {len, (char*)data};
        OVERLAPPED* ovl_p = &(((struct session_ext_s*)session->ext_data)->ovl_send.base);
        ovl_p->OffsetHigh   = len;
        send_len = len;

        if(SOCKET_ERROR == (WSASend(session->fd, &out_buf, 1, (LPDWORD)&send_len, 0, ovl_p, 0)))
        {
            if(sErrno == WSA_IO_PENDING)
            {
                /*  设置pending标志位为true   */
                session->send_ispending = true;
            }
            else
            {
                send_len = -1;
            }
        }
    }

    return send_len;
}

/*  TODO::不管是epoll和iocp是否该提供预先序列化数据,然后再直接发送缓冲区    */
static int iocp_wrap_session_senddata(struct session_s* session, const char* data, int len)
{
    struct buffer_s* temp_buffer = session->send_buffer;
    int send_len = 0;
    int temp_size = 0;

    /*  如果session处于pending或者copy到内置缓冲区失败则返回0   */
    if(session->send_ispending || (data != NULL && !ox_buffer_write(temp_buffer, data, len)))
    {
        return 0;
    }

    /*  获取可读(待发送)数据  */
    temp_size = ox_buffer_getreadvalidcount(temp_buffer);
    if(temp_size > 0)
    {
        /*  投递待发送数据 */
        send_len = iocp_session_senddata(session, ox_buffer_getreadptr(temp_buffer), temp_size);
        if(send_len > 0)
        {
            /*  投递(无论pending与否)成功清除数据    */
            ox_buffer_addreadpos(temp_buffer, temp_size);
        }
    }

    return send_len;
}

static bool iocp_send_complete(struct iocp_s* iocp, struct session_s* session, struct session_ext_s* ext_p, int bytes)
{
    int ret = 0;

    session->send_ispending = false;
    session->is_use = false;

    if(iocp->base.logic_on_sendfinish)
    {
        /*  调用上层发送完成通知  */
        /*  用于上层采用sendv接口时,调用此函数直接路由给上层进行下一步处理,此处不做任何其他操作   */
        (iocp->base.logic_on_sendfinish)(&iocp->base, session->index, bytes);
    }   /*  TODO::考证    */
    else
    {
        ret = iocp_wrap_session_senddata(session, NULL, 0);

        if(ret >= 0 && !session->send_ispending && session->haveleftdata)
        {
            /*  如果socket仍然可写且逻辑层有未发送的数据则通知逻辑层    */
            struct server_s* server = &iocp->base;
            (server->logic_on_cansend)(server, session->index);
        }
    }

    return  ret >= 0;
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
        must_close = !iocp_send_complete(iocp, session, ext_p, bytes);
    }

    if(must_close)
    {
        iocp_session_onclose(iocp, session);
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
    CopyMemory(session->ip, ip, strlen(ip));

    ox_buffer_init(session->recv_buffer);
    ox_buffer_init(session->send_buffer);

    session->active = true;
    session->is_use = false;

    ox_socket_nonblock(session->fd);

    CreateIoCompletionPort((HANDLE)session->fd, iocp->iocp_handle, (DWORD)&(ext->ck), 0);

    if(!iocp_wait_recv(session, ext, session->fd))
    {
        iocp_session_reset(session);
        printf("accept %d recv error, %d\n", session->fd, sErrno);
        return false;
    }

    (iocp->base.logic_on_enter)(&iocp->base, session->index);

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
            if(true)
            {
                iocp_session_onclose(iocp, session_p);
            }
        }
        else
        {
            ext_s = (struct session_ext_s*)session_p->ext_data;
            iocp_iocomplete(iocp, session_p, ext_s, ovl_p->base.Offset, Bytes);
        }

        current_time = ox_getnowtime();
        ms = end_time - current_time;
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
    logic_on_enter_pt enter_pt,
    logic_on_close_pt close_pt,
    logic_on_recved_pt   recved_pt,
    logic_on_cansend_pt cansend_pt,
    logic_on_sendfinish_pt  sendfinish_pt
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
        iocp->base.logic_on_close = close_pt;
        iocp->base.logic_on_recved = recved_pt;
        iocp->base.logic_on_cansend = cansend_pt;
        iocp->base.logic_on_sendfinish = sendfinish_pt;

        iocp->exts = (struct session_ext_s*)malloc(sizeof(struct session_ext_s) * iocp->max_num);
        iocp->sessions = (struct session_s*)malloc(sizeof(struct session_s) * iocp->max_num);

        memset(iocp->sessions, 0, sizeof(struct session_s) * iocp->max_num);

        iocp->freelist = ox_stack_new(iocp->max_num, sizeof(struct session_s*));

        for (; i < iocp->max_num; ++i)
        {
            struct session_s* session = iocp->sessions+i;
            session->ext_data = iocp->exts+i;
            session->index = i;
            iocp_session_init(iocp, session);
            session->active = false;

            ox_stack_push(iocp->freelist, &session);
            iocp->freelist_num++;
        }

        iocp->run_flag = true;
    }
}

static void iocp_stop_callback(struct server_s* self)
{
    struct iocp_s* iocp = (struct iocp_s*)self;
    if(iocp->run_flag)
    {
        int i = 0;

        for(; i < iocp->max_num; ++i)
        {
            ox_socket_close(iocp->sessions[i].fd);
        }

        i = 0;
        for(; i < iocp->max_num; ++i)
        {
            struct session_s* session = iocp->sessions+i;
            ox_buffer_delete(session->recv_buffer);
            ox_buffer_delete(session->send_buffer);

            session->recv_buffer = NULL;
            session->send_buffer = NULL;
        }

        ox_stack_delete(iocp->freelist);
        free(iocp->sessions);
        iocp->sessions = NULL;

        free(iocp->exts);
        iocp->exts = NULL;

        CloseHandle(iocp->iocp_handle);
        iocp->iocp_handle = NULL;
        iocp->run_flag = false;

        free(iocp);
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
            send_len = iocp_wrap_session_senddata(session, data, len);
        }

        /*  如果没有完全发送完数据则设置标志    */
        if(!session->haveleftdata)
        {
            session->haveleftdata = (send_len >= 0 && send_len < len);
        }
    }

    return send_len;
}

/*  返回-1表示socket断开,否则返回0  */
static int iocp_sendv_callback(struct server_s* self, int index, const char* datas[], const int* lens, int num)
{
    /*  同一时刻只能存在一次wsasend   */
    int send_len = 0;
    struct iocp_s* iocp = (struct iocp_s*)self;
    if(index >= 0 && index < iocp->max_num && num > 0 && num <= MAX_SENDBUF_NUM)
    {
        struct session_s* session = iocp->sessions+index;

        if(session->active && !session->is_use)
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
                    printf("%d WSASend send failed, error id: %d\n", index, sErrno);
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
            session->is_use = true;
        }
    }

    return send_len;
}

static int iocp_copy_callback(struct server_s* self, int index, const char* data, int len)
{
    if(data != NULL && len > 0)
    {
        struct iocp_s* iocp = (struct iocp_s*)self;
        if(index >= 0 && index < iocp->max_num)
        {
            struct session_s* session = iocp->sessions+index;
            if(session->active && !session->send_ispending)
            {
                if(ox_buffer_write(session->send_buffer, data, len))
                {
                    return len;
                }
            }
        }
    }

    return 0;
}

static void iocp_closesession_callback(struct server_s* self, int index)
{
    struct iocp_s* iocp = (struct iocp_s*)self;
    struct session_s* session = iocp->sessions+index;
    
    if(session->fd != SOCKET_ERROR)
    {
        iocp_session_reset(session);
        ox_stack_push(iocp->freelist, &session);
        iocp->freelist_num++;
    }
}

static bool iocp_register_callback(struct server_s* self, int fd)
{
    struct iocp_s* iocp = (struct iocp_s*)self;
    if(iocp->freelist_num > 0)
    {
        struct session_s** ppsession = (struct session_s**)ox_stack_popback(iocp->freelist);
        if(ppsession != NULL)
        {
            struct session_s* session = *ppsession;
            iocp->freelist_num--;

            session->fd = fd;
            
            if(!iocp_accept_complete(iocp, session))
            {
                ox_stack_push(iocp->freelist, ppsession);
                iocp->freelist_num++;
                return false;
            }
            else
            {
                return true;
            }
        }
    }
    else
    {
        printf("register fd error, because res no enough");
    }

    return false;
}

struct server_s* iocp_create(
    int max_num,
    int session_recvbuffer_size,
    int session_sendbuffer_size,
    void*   ext)
{
    struct iocp_s* iocp = (struct iocp_s*)malloc(sizeof(struct iocp_s));
    memset(iocp, 0, sizeof(*iocp));

    iocp->base.start_pt = iocp_start_callback;
    iocp->base.poll_pt = iocp_poll_callback;
    iocp->base.stop_pt = iocp_stop_callback;
    iocp->base.closesession_pt = iocp_closesession_callback;
    iocp->base.register_pt = iocp_register_callback;
    iocp->base.send_pt = iocp_send_callback;
    iocp->base.sendv_pt = iocp_sendv_callback;
    iocp->base.copy_pt = iocp_copy_callback;

    iocp->base.ext = ext;
    iocp->run_flag = false;
    iocp->max_num = max_num;

    iocp->session_recvbuffer_size = session_recvbuffer_size;
    iocp->session_sendbuffer_size = session_sendbuffer_size;

#if permormance
    iocp->send_count = 0;
    iocp->total_recv_len = 0;
    iocp->total_send_len = 0;
    iocp->old_time = 0;
#endif
    return &iocp->base;
}

void iocp_delete(struct server_s* self)
{
    iocp_stop_callback(self);
    free(self);
}
