#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "socketlibtypes.h"
#include "socketlibfunction.h"
#include "buffer.h"
#include "rwlist.h"
#include "typepool.h"
#include "stack.h"
#include "fdset.h"
#include "systemlib.h"
#include "multipool.h"
#include "thread.h"

#include "connection.h"

#define DF_LIST_SIZE (1024)
#define PROC_LIST_SIZE (10)

enum connection_status
{
    connection_none,         /*  未链接状态   */
    connection_establish,    /*  链接状态    */
};

enum send_msg_type
{
    send_msg_connect,
    send_msg_close,
    send_msg_data,
};

struct connection_s
{
    sock fd;
    enum connection_status status;

    struct fdset_s*         fdset;

    struct buffer_s*        send_buffer;
    struct rwlist_s*        sendmsg_list;       /*  逻辑层投递给网络层的消息队列   */
    struct rwlist_s*        free_sendmsg_list;  /*  网络层已经处理的消息队列(需要逻辑层回收)   */

    struct buffer_s*        recv_buffer;
    struct rwlist_s*        netmsg_list;        /*  网络层投递给逻辑层的消息队列  */
    struct rwlist_s*        free_netmsg_list;   /*  逻辑层已经处理的消息队列(需要网络层回收)   */

    struct rwlist_s*        proc_list;

    pfn_check_packet check;
    pfn_packet_handle handle;

    bool    writable;

    void*   ext;

    int     current_msg_id;
    int     last_send_msg_id;

    int     total_recv_len;
};

#define DF_RWLIST_PENDING_NUM (5)

struct connection_s*
ox_connection_new(int rdsize, int sdsize, pfn_check_packet check, pfn_packet_handle handle, void* ext)
{
    struct connection_s* ret = (struct connection_s*)malloc(sizeof(*ret));

    ret->fd = SOCKET_ERROR;
    ret->status = connection_none;
    ret->fdset = ox_fdset_new();

    ret->send_buffer = ox_buffer_new(sdsize);

    ret->sendmsg_list = ox_rwlist_new(DF_LIST_SIZE, sizeof(struct msg_data_s*), DF_RWLIST_PENDING_NUM);
    ret->free_sendmsg_list = ox_rwlist_new(DF_LIST_SIZE, sizeof(struct msg_data_s*), DF_RWLIST_PENDING_NUM);

    ret->recv_buffer = ox_buffer_new(rdsize);

    ret->netmsg_list = ox_rwlist_new(DF_LIST_SIZE, sizeof(struct msg_data_s*), DF_RWLIST_PENDING_NUM);
    ret->free_netmsg_list = ox_rwlist_new(DF_LIST_SIZE, sizeof(struct msg_data_s*), DF_RWLIST_PENDING_NUM);

    ret->proc_list = ox_rwlist_new(PROC_LIST_SIZE, sizeof(struct msg_data_s*), DF_RWLIST_PENDING_NUM);

    ret->check = check;
    ret->handle = handle;

    ret->ext = ext;
    ret->writable = true;
    ret->current_msg_id = 0;
    ret->last_send_msg_id = 0;

    return ret;
}

void
ox_connection_delete(struct connection_s* self)
{
    ox_fdset_delete(self->fdset);
    ox_buffer_delete(self->send_buffer);
    ox_buffer_delete(self->recv_buffer);

    ox_rwlist_delete(self->sendmsg_list);
    ox_rwlist_delete(self->free_sendmsg_list);
    ox_rwlist_delete(self->netmsg_list);
    ox_rwlist_delete(self->free_netmsg_list);
    ox_rwlist_delete(self->proc_list);

    if(self->fd != SOCKET_ERROR)
    {
        ox_socket_close(self->fd);
        self->fd = SOCKET_ERROR;
    }

    free(self);
}

struct msg_data_s*
ox_connection_sendmsg_claim(struct connection_s* self, int len)
{
    return (struct msg_data_s*)malloc(sizeof(struct msg_data_s)+len);
}

void
ox_connection_send(struct connection_s* self, struct msg_data_s* msg)
{
    /*  逻辑层请求网络层发送数据(投递消息给网络层)  */
    msg->type = send_msg_data;

    self->current_msg_id++;
    msg->msg_id = self->current_msg_id;
    
    ox_rwlist_push(self->sendmsg_list, &msg);
}

#pragma pack(push)
#pragma pack(1)
struct connect_msg
{
    char    ip[IP_SIZE];
    int port;
    int timeout;
};
#pragma pack(pop)

void
ox_connection_send_connect(struct connection_s* self, const char* ip, int port, int timeout)
{
    struct msg_data_s* msg = (struct msg_data_s*)malloc(sizeof(struct msg_data_s)+sizeof(struct connect_msg));
    if(msg != NULL)
    {
        struct connect_msg* connect_data = (struct connect_msg*)msg->data;
        msg->type = send_msg_connect;
        msg->len = 0;

        memcpy(connect_data->ip, ip, strlen(ip)+1);
        connect_data->port = port;
        connect_data->timeout = timeout;

        ox_rwlist_push(self->proc_list, &msg);
        ox_rwlist_force_flush(self->proc_list);
    }
}

void
ox_connection_send_close(struct connection_s* self)
{
    struct msg_data_s* msg = (struct msg_data_s*)malloc(sizeof(struct msg_data_s));
    if(msg != NULL)
    {
        msg->type = send_msg_close;
        msg->len = 0;
        ox_rwlist_push(self->proc_list, &msg);
        ox_rwlist_force_flush(self->proc_list);
    }
}

static void
connection_close(struct connection_s* self)
{
    if(self->fd != SOCKET_ERROR)
    {
        ox_socket_close(self->fd);
        ox_fdset_del(self->fdset, self->fd, ReadCheck || WriteCheck);
        self->fd = SOCKET_ERROR;
        self->status = connection_none;
        self->writable = false;
        ox_buffer_init(self->recv_buffer);
        ox_buffer_init(self->send_buffer);
        self->current_msg_id = 0;
        self->last_send_msg_id = 0;
    }
}

static void
connection_send_logicmsg(struct connection_s* self, enum net_msg_type type, const char* data, int data_len)
{
    struct msg_data_s* msg = (struct msg_data_s*)malloc(data_len+sizeof(struct msg_data_s));
    assert(msg != NULL);

    if(msg != NULL)
    {
        /*  网络层投递消息给逻辑层 */
        msg->type = type;
        msg->len = data_len;

        if(data != NULL)
        {
            memcpy(msg->data, data, data_len);
        }

        ox_rwlist_push(self->netmsg_list, &msg);
    }
}

static sock
socket_non_connect(const char* server_ip, int port, int overtime)
{
    struct sockaddr_in server_addr;
    sock clientfd = SOCKET_ERROR;
    bool connect_ret = true;

    ox_socket_init();

    clientfd = socket(AF_INET, SOCK_STREAM, 0);
    ox_socket_nonblock(clientfd);

    if(clientfd != SOCKET_ERROR)
    {
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = inet_addr(server_ip);
        server_addr.sin_port = htons(port);

        if(connect(clientfd, (struct sockaddr*)&server_addr, sizeof(struct sockaddr)) < 0)
        {
            int check_error = 0;
            
            #if defined PLATFORM_WINDOWS
            check_error = WSAEWOULDBLOCK;
            #else
            check_error = EINPROGRESS;
            #endif

            if(check_error != sErrno)
            {
                connect_ret = false;
            }
            else
            {
                struct fdset_s* fdset = ox_fdset_new();
                bool canwrite = false;
                bool canread = false;
                /*  重设默认链接状态为失败   */
                connect_ret = false;

                ox_fdset_add(fdset, clientfd, ReadCheck | WriteCheck);
                ox_fdset_poll(fdset, overtime);

                canwrite = ox_fdset_check(fdset, clientfd, WriteCheck);
                canread = ox_fdset_check(fdset, clientfd, ReadCheck);

                if(canwrite)
                {
                    if(canread)
                    {
                        int error;
                        int len = sizeof(error);
                        if(getsockopt(clientfd, SOL_SOCKET, SO_ERROR, (char*)&error, (socklen_t*)&len) >= 0)
                        {
                            /*  返回值不为-1(linux下失败也返回0,后续会检测出来)即为成功 */
                            connect_ret = true;
                        }
                    }
                    else
                    {
                        /*  可读且不可写即为链接成功    */
                        connect_ret = true;
                    }
                }

                ox_fdset_delete(fdset);
            }
        }
    }

    if(!connect_ret)
    {
        ox_socket_close(clientfd);
        clientfd = SOCKET_ERROR;
    }

    return clientfd;
}

void
connection_connect_help(struct connection_s* self, struct connect_msg* connect_data)
{
    if(self->fd != SOCKET_ERROR)
    {
        return;
    }

    self->fd = socket_non_connect(connect_data->ip, connect_data->port, connect_data->timeout);
    if(self->fd != SOCKET_ERROR)
    {
        ox_socket_nodelay(self->fd);
        ox_fdset_add(self->fdset, self->fd, ReadCheck);
        ox_socket_nonblock(self->fd);
        self->status = connection_establish;
    }

    connection_send_logicmsg(self, (self->status == connection_none ? net_msg_connect_failed : net_msg_establish), NULL, 0);
}

static void
connection_free_sendmsglist(struct connection_s* self)
{
    struct rwlist_s*    free_sendmsg_list = self->free_sendmsg_list;
    struct msg_data_s** msg_p = NULL;

    while((msg_p = (struct msg_data_s**)ox_rwlist_pop(free_sendmsg_list, 0)) != NULL)
    {
        struct msg_data_s* msg = *msg_p;
        free(msg);
    }
}

static void
connection_free_netmsglist(struct connection_s* self)
{
    struct rwlist_s* freenetmsg_list = self->free_netmsg_list;
    struct msg_data_s** msg_p = NULL;

    while((msg_p = (struct msg_data_s**)ox_rwlist_pop(freenetmsg_list, 0)) != NULL)
    {
        struct msg_data_s* msg = *msg_p;
        free(msg);
    }
}

static void
connection_sendmsg_handle(struct connection_s* self)
{
    if(self->status == connection_establish)
    {
        struct buffer_s* send_buffer = self->send_buffer;
        int send_len = 0;

proc_send:
        {
            struct rwlist_s* free_sendmsg_list = self->free_sendmsg_list;
            struct rwlist_s* sendmsg_list = self->sendmsg_list;
            struct msg_data_s** msg = NULL;

            while((msg = (struct msg_data_s**)ox_rwlist_front(sendmsg_list, 0)) != NULL)
            {
                struct msg_data_s* data = *msg;

                /*  只处理发送数据的消息  */
                if(data->type == send_msg_data)
                {
                    /*  先写入内置缓冲区    */
                    if(ox_buffer_write(send_buffer, data->data, data->len))
                    {
                        assert(data->msg_id == self->last_send_msg_id+1);
                        self->last_send_msg_id++;

                        ox_rwlist_pop(sendmsg_list, 0);
                        ox_rwlist_push(free_sendmsg_list, msg);
                    }
                    else
                    {
                        break;
                    }
                }
                else
                {
                    ox_rwlist_pop(sendmsg_list, 0);
                    ox_rwlist_push(free_sendmsg_list, msg);
                }
            }
        }

        send_len = ox_buffer_getreadvalidcount(send_buffer);
        if(send_len > 0)
        {
            /*  发送内置缓冲区 */
            int ret_len = send(self->fd, ox_buffer_getreadptr(send_buffer), send_len, 0);
            if(ret_len > 0)
            {
                ox_buffer_addreadpos(send_buffer, ret_len);

                if(send_len == ret_len)
                {
                    /*  如果发送全部成功,则继续处理发送消息队列中的消息    */
                    goto proc_send;
                }
            }
            else if(ret_len == SOCKET_ERROR)
            {
                if(S_EWOULDBLOCK == sErrno)
                {
                    self->writable = false;
                    ox_fdset_add(self->fdset, self->fd, WriteCheck);
                }
            }
        }
    }
}

static int
connection_split_packet(struct connection_s* self)
{
    int proc_len = 0;
    struct buffer_s* recv_buffer = self->recv_buffer;
    const char* buffer = ox_buffer_getreadptr(recv_buffer);
    int left_len = ox_buffer_getreadvalidcount(recv_buffer);

    while(left_len > 0)
    {
        /*  调用验证包完整回调函数:如果收到完整包则投递到逻辑层  */
        int packet_len = (self->check)(buffer, left_len);
        if(packet_len > 0)
        {
            connection_send_logicmsg(self, net_msg_data, buffer, packet_len);

            buffer += packet_len;
            left_len -= packet_len;
            proc_len += packet_len;
        }
        else
        {
            break;
        }
    }

    return proc_len;
}

static void
connection_read_handle(struct connection_s* self)
{
    struct buffer_s* recv_buffer =self->recv_buffer;
    bool is_close = false;
    bool proc_begin = false;
    int all_recv_len = 0;

procbegin:
    proc_begin = false;
    all_recv_len = 0;

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
            recv_len = recv(self->fd, ox_buffer_getwriteptr(recv_buffer), can_recvlen, 0);
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
                ox_buffer_addwritepos(recv_buffer, recv_len);
                all_recv_len += recv_len;

                if(recv_len == can_recvlen)
                {
                    /*  如果全部读取,则设置标志:处理完刚接收的数据后应该继续读取 */
                    proc_begin = true;
                }
            }
        }

        break;
    }

    if(is_close)
    {
        connection_send_logicmsg(self, net_msg_close, NULL, 0);
        connection_close(self);
    }
    else if(all_recv_len > 0)
    {
        int len = connection_split_packet(self);
        ox_buffer_addreadpos(recv_buffer, len);
    }

    if(proc_begin && self->fd != SOCKET_ERROR)
    {
        goto procbegin;
    }
}

static void
connection_proclist_handle(struct connection_s* self)
{
    struct rwlist_s* proc_list = self->proc_list;
    struct msg_data_s** pmsg = NULL;

    while((pmsg = (struct msg_data_s**)ox_rwlist_pop(proc_list, 0)) != NULL)
    {
        struct msg_data_s* msg = *pmsg;
        if(msg->type == send_msg_connect)
        {
            connection_connect_help(self, (struct connect_msg*)msg->data);
        }
        else if(msg->type == send_msg_close)
        {
            connection_close(self);
        }

        free(msg);
    }
}

void
ox_connection_netpoll(struct connection_s* self, int64_t millisecond)
{
    if(millisecond < 0)
    {
        millisecond = 0;
    }

    /*  网络层调度函数:如果当前状态是未链接则尝试取出消息并链接服务器,否则处理来自逻辑层的消息队列    */
    if(self->status == connection_none)
    {
        ox_thread_sleep(millisecond);
        connection_proclist_handle(self);
    }
    else
    {
        int64_t current_time = ox_getnowtime();
        const int64_t end_time = current_time + millisecond;

        do
        {
            if(ox_fdset_poll(self->fdset, end_time-current_time) > 0)
            {
                if(ox_fdset_check(self->fdset, self->fd, ReadCheck))
                {
                    connection_read_handle(self);
                }

                if(self->fd != SOCKET_ERROR && ox_fdset_check(self->fdset, self->fd, WriteCheck))
                {
                    self->writable = true;
                    ox_fdset_del(self->fdset, self->fd, WriteCheck);
                }
            }

            if(self->writable)
            {
                connection_sendmsg_handle(self);
            }
            current_time = ox_getnowtime();
        }while(end_time > current_time);

        connection_proclist_handle(self);
    }
    
    ox_rwlist_flush(self->netmsg_list);
    connection_free_netmsglist(self);
}

void
ox_connection_logicpoll(struct connection_s* self, int64_t millisecond)
{
    /*  逻辑层调度函数:处理来自网络层投递的网络消息,以及释放逻辑层投递出的消息  */
    struct rwlist_s* logicmsg_list = self->netmsg_list;
    struct msg_data_s** msg_p = NULL;
    struct msg_data_s* msg = NULL;
    pfn_packet_handle handle = self->handle;
    void* ext = self->ext;
    struct rwlist_s*    free_netmsg_list = self->free_netmsg_list;
    int64_t current_time = ox_getnowtime();
    int64_t end_time = current_time;
    if(millisecond < 0)
    {
        millisecond = 0;
    }

    end_time += millisecond;

    do 
    {
        msg_p = (struct msg_data_s**)ox_rwlist_pop(logicmsg_list, end_time-current_time);
        if(msg_p != NULL)
        {
            msg = *msg_p;
            (handle)(self, msg, ext);
            ox_rwlist_push(free_netmsg_list, msg_p);

            current_time = ox_getnowtime();
        }
        else
        {
            break;
        }
    }while(end_time > current_time);

    ox_rwlist_flush(self->sendmsg_list);
    connection_free_sendmsglist(self);
}
