#include "iocp.h"
#include "server.h"

struct packet_head_s
{
    /*  包长度(包含包头自身) */
    short   len;
    short   op;
};

static struct server_s* server = 0;

void gateway_init(int port)
{
    server = iocp_create(port, 4096, 4096, 1024);
}

static void my_logic_on_enter_pt(struct server_s* self, int index)
{

}

static void my_logic_on_close_pt(struct server_s* self, int index)
{

}

int my_pfn_check_packet(const char* buffer, int len)
{
    int ret_len = 0;
    if(len >= sizeof(struct packet_head_s))
    {
        struct packet_head_s* head = (struct packet_head_s*)buffer;
        if(len >= head->len)
        {
            ret_len = head->len;
        }
    }

    return ret_len;
}

static char buff[1024];

static int my_logic_on_recved_pt(struct server_s* self, int index, const char* buffer, int len)
{
    int proc_len = 0;
    int left_len = len;
    const char* check_buffer = buffer;

    while(left_len > 0)
    {
        int check_len = my_pfn_check_packet(check_buffer, left_len);
        if(check_len > 0)
        {
            {
                /*  返回消息包给客户端   */
                struct packet_head_s* head = (struct packet_head_s*)buff;
                head->len = 10;

                server_send(self, index, buff, 10);
            }
            check_buffer += check_len;
            left_len -= check_len;
            proc_len += check_len;
        }
        else
        {
            break;
        }
    }

    return proc_len;
}

void gateway_start()
{
    /*  开启多个线程等待IOCP,有事件后会调用传入的回调函数   */
    server_start(server, my_logic_on_enter_pt, my_logic_on_close_pt, my_logic_on_recved_pt);
}

