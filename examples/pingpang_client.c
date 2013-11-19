#include <stdio.h>

#include "thread.h"
#include "socketlibfunction.h"
#include "thread_reactor.h"

#define SERVER_PORT 4100
#define CLIENT_NUM 5000
#define PACKET_LEN (16*1024)

static int s_check(void* ud, const char* buffer, int len)
{
    if(len >= PACKET_LEN)
	{
		return PACKET_LEN;
	}
	else 
	{
		return 0;
	}
}

static void msg_handle(struct nr_mgr* mgr, struct nrmgr_net_msg* msg)
{
    if(msg->type == nrmgr_net_msg_connect)
    {
        {
            struct nrmgr_send_msg_data* sd_msg = ox_nrmgr_make_sendmsg(mgr, NULL, PACKET_LEN);
            sd_msg->data_len = PACKET_LEN;
            /*  发送消息    */
            ox_nrmgr_sendmsg(mgr, sd_msg, msg->session);
        }
        {
            struct nrmgr_send_msg_data* sd_msg = ox_nrmgr_make_sendmsg(mgr, NULL, PACKET_LEN);
            sd_msg->data_len = PACKET_LEN;
            /*  发送消息    */
            ox_nrmgr_sendmsg(mgr, sd_msg, msg->session);
        }
		printf("connection enter  \n");
    }
    else if(msg->type == nrmgr_net_msg_close)
    {
        printf("connection close , close session \n");
        ox_nrmgr_closesession(mgr, msg->session);
    }
    else if(msg->type == nrmgr_net_msg_data)
    {
        /*  申请发送消息  */
        struct nrmgr_send_msg_data* sd_msg = ox_nrmgr_make_sendmsg(mgr, NULL, PACKET_LEN);
		memcpy(sd_msg->data, msg->data, msg->data_len);
        sd_msg->data_len = msg->data_len;
        /*  发送消息    */
        ox_nrmgr_sendmsg(mgr, sd_msg, msg->session);
    }
}

int main()
{
	struct nr_mgr* mgr = ox_create_nrmgr(1, PACKET_LEN*2, s_check);
	
	{
		int i = 0;
		for(; i < CLIENT_NUM; ++i)
		{
			sock fd = ox_socket_connect("127.0.0.1", SERVER_PORT);
			if(fd != SOCKET_ERROR)
			{
				ox_nrmgr_addfd(mgr, NULL, fd);
			}
		}
	}

    while(true)
    {
        ox_nrmgr_logic_poll(mgr, msg_handle, 5);
    }
	return 0;
}