#include <string.h>
#include <stdlib.h>

#include "vld.h"

#include "st_server.h"
#include "thread.h"
#include "socketlibtypes.h"
#include "socketlibfunction.h"

#define PACKET_SIZE 20

enum pool_type
{
    packet_pool_tiny,
    packet_pool_middle,
    packet_pool_big,
};

struct packet_head_s
{
    /*  包长度(包含包头自身) */
    short   len;
    short   op;
};

/*  检查数据是否含有完整包 */
int my_pfn_check_packet(const char* buffer, int len)
{
    int ret_len = 0;

    if(len >= PACKET_SIZE)
    {
        ret_len = len;
    }
    //if(len >= sizeof(struct packet_head_s))
    //{
    //    struct packet_head_s* head = (struct packet_head_s*)buffer;
    //    if(len >= head->len)
    //    {
    //        ret_len = head->len;
    //    }
    //}

    return ret_len;
}

#define TEST_ECHO ("<html></html>")

int all_recved_packet_num = 0;
char buffer[1024];

/*  包处理函数   */
void my_pfn_packet_handle(struct st_server_s* self, int index, const char* buffer, int len)
{
    /*  分配packet_pool_tiny类型的包,构造数据 */
    int i = 0;
    for(; i < 1; ++i)
    {
        struct st_server_packet_s* msg = ox_stserver_claim_packet(self, packet_pool_big);
       /* int msg_len = strlen(TEST_ECHO)+1;
        struct packet_head_s* head = (struct packet_head_s*)msg->data;

        memcpy(msg->data+sizeof(*head), TEST_ECHO, msg_len);
        msg->data_len = msg_len+sizeof(*head);
        head->len = msg->data_len;*/

        memcpy(msg->data, buffer, len);
        msg->data_len = len;

        ox_stserver_send(self, msg, index);

        /*  发送完毕后必须(尝试)释放包  */
        ox_stserver_check_reclaim(self, msg);
    }

    all_recved_packet_num += 1;

    ox_stserver_flush(self, index);
}

/*  客户端连接进入时的回调函数   */
void my_pfn_session_onenter(struct st_server_s* self, int index)
{
    printf("client enter, index:%d\n", index);
}

/*  客户端断开时的回调函数,某些情况下网络层无法检测到客户端断开,需要逻辑层做心跳检测 */
void my_pfn_session_onclose(struct st_server_s* self, int index)
{
    printf("client close, index:%d\n", index);
}

void print_thread(void *arg)
{
    int old = all_recved_packet_num;
    while(1)
    {
        old = all_recved_packet_num;
        ox_thread_sleep(1000);
        printf("recv %d packet /s\n", (all_recved_packet_num-old));
    }
}

static void
    listen_thread_fun(void* arg)
{
    struct st_server_s* st = (struct st_server_s*)arg;
    sock client_fd = SOCKET_ERROR;
    struct sockaddr_in socketaddress;
    socklen_t size = sizeof(struct sockaddr);

    sock listen_fd = ox_socket_listen(4002, 25);

    if(SOCKET_ERROR != listen_fd)
    {
        for(;1;)
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
                ox_socket_nodelay(client_fd);
                ox_stserver_addfd(st, client_fd);
            }
        }

        listen_fd = SOCKET_ERROR;
    }
    else
    {
        printf("listen failed\n");
    }
}

/*  使用server-model02中的基础网络库 : 单线程模型---既可以作为客户端开发也可以作为服务器   */
/*  消息包直接在网络事件中处理  */

/*  网关服没有使用thread_reactor.c (内部开启多个独立的reactor),因为它是基于消息队列的,必须有一个逻辑线程去处理这个队列,而且存在多个reactor之间的通信
所以怕影响效率   */

static const int nums[] = {128, 128, 2048};
static const int lens[] = {128, 128, 128};

int main()
{
    struct st_server_msgpool_config config = {nums, lens, 3};

    struct st_server_s* gs = ox_stserver_create(2048, 1024, 1024, my_pfn_check_packet, my_pfn_packet_handle, my_pfn_session_onenter, my_pfn_session_onclose, config);
    ox_thread_new(print_thread, NULL);

    /*  开启listen线程，接受客户端链接  */
    ox_thread_new(listen_thread_fun, gs);
    while(1)
    {
        ox_stserver_poll(gs, 10);
        ox_stserver_flush(gs, 2047);
    }

    ox_stserver_delete(gs);
    return 0;
}