#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "connection.h"
#include "thread.h"

static struct connection_s* client = NULL;

static int total_send = 0;

static int s_check(const char* buffer, int len)
{
	return len;
}

static void s_send()
{
	struct msg_data_s* sd_msg = ox_connection_sendmsg_claim(client, 0);
	memcpy(sd_msg->data, "testtesttest", 5);
	sd_msg->len = 5;
	ox_connection_send(client, sd_msg);
	total_send++;
}

static void s_handle(struct connection_s* self, struct msg_data_s* msg, void* ext)
{
	if(msg->type == net_msg_establish)
	{
		s_send();
	}
	else if(msg->type == net_msg_data)
	{
		s_send();
	}
}

static void s_netthread(void* arg)
{
	while(true)
	{
		ox_connection_netpoll(client, 1);
	}
}

int main()
{
	int nums[] = {1280, 1280, 1280};
	int lens[] = {128, 128, 128};
	int s_time = ox_getnowtime();

	struct connection_msgpool_config config = 
	{
		nums,
		lens,
		3,
		nums,
		lens,
		3
	};

	client = ox_connection_new(1024, 1024, s_check, s_handle, config, NULL);
	ox_thread_new(s_netthread, NULL);
	ox_connection_send_connect(client, "127.0.0.1", 4002, 10000);

	while(true)
	{
		ox_connection_logicpoll(client, 1);
		
		{
			int nowtime = ox_getnowtime();
			if((nowtime-s_time) >= 1000)
			{
				s_time = nowtime;
				printf("total recv %d msg/s\n", total_send);
				total_send = 0;
			}
		}
	}

	return 0;
}
