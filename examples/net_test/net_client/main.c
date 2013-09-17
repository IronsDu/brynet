#include <string.h>
#include <stdlib.h>

#include "st_server.h"
#include "thread.h"
#include "socketlibtypes.h"
#include "socketlibfunction.h"

#define PACKET_SIZE 20

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
        ret_len = PACKET_SIZE;
    }

    return ret_len;
}

#define TEST_ECHO ("<html></html>")

int all_recved_packet_num = 0;
char buffer[1024];

enum pool_type
{
    packet_pool_tiny,
    packet_pool_middle,
    packet_pool_big,
};

static int gflag_num = 0;

/*  包处理函数   */
void my_pfn_packet_handle(struct st_server_s* self, int index, const char* buffer, int len)
{
    /*  分配packet_pool_tiny类型的包,构造数据 */
    if(true)//++gflag_num == 1)
    {
        struct st_server_packet_s* msg = ox_stserver_claim_packet(self, packet_pool_big);
        msg->data_len = PACKET_SIZE;

        ox_stserver_send(self, msg, index);
        ox_stserver_flush(self, index);

        /*  发送完毕后必须(尝试)释放包  */
        ox_stserver_check_reclaim(self, msg);

        gflag_num= 0 ;
    }

    all_recved_packet_num += 1;
}

/*  客户端连接进入时的回调函数   */
void my_pfn_session_onenter(struct st_server_s* self, int index)
{
    if(false)
    {
        /*  客户端链接成功到服务器后，立即发送消息报    */

        /*  分配packet_pool_tiny类型的包,构造数据 */
        struct st_server_packet_s* msg = ox_stserver_claim_packet(self, packet_pool_big);
        //int msg_len = strlen(TEST_ECHO)+1;
        //struct packet_head_s* head = (struct packet_head_s*)msg->data;

        //memcpy(msg->data+sizeof(*head), TEST_ECHO, msg_len);
        //msg->data_len = msg_len+sizeof(*head);
        //head->len = msg->data_len;
        msg->data_len = PACKET_SIZE;

        ox_stserver_send(self, msg, index);
        ox_stserver_flush(self, index);

        /*  发送完毕后必须(尝试)释放包  */
        ox_stserver_check_reclaim(self, msg);

        printf("client enter, index:%d\n", index);
    }
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

#define MAX_CLIENT_NUM 1

static void
connect_thread_fun(void* arg)
{
    struct st_server_s* st = (struct st_server_s*)arg;
    int i = 0;

    for(; i < MAX_CLIENT_NUM; ++i)
    {
        sock clientfd = ox_socket_connect("192.168.1.88", 4002);
        if(SOCKET_ERROR != clientfd)
        {
            ox_socket_nodelay(clientfd);
            ox_stserver_addfd(st, clientfd);
        }
    }
}

/*  使用server-model02中的基础网络库 : 单线程模型---既可以作为客户端开发也可以作为服务器   */
/*  消息包直接在网络事件中处理  */

/*  本文件作为客户端    */

static const int config_packet_num[] = {128, 128, 2048};
static const int config_packet_len[] = {128, 128, 128};

#define ARRAY_COUNT(X) (sizeof(X)/sizeof(X[0]))
int main()
{
    struct st_server_msgpool_config config = {config_packet_num, config_packet_len, ARRAY_COUNT(config_packet_num)};
    struct st_server_s* gs = ox_stserver_create(1024, 1024, 1024, my_pfn_check_packet, my_pfn_packet_handle, my_pfn_session_onenter, my_pfn_session_onclose, config);
    ox_thread_new(print_thread, NULL);

    /*  开启connect线程，链接服务器 */
    ox_thread_new(connect_thread_fun, gs);

    while(1)
    {
        ox_stserver_poll(gs, 10);
    }
    return 0;
}