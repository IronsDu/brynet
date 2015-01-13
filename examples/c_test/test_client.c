#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "connection.h"

static int s_check(const char* buffer, int len)
{
	return len;
}

static void s_msg_handle(struct connection_s* self, struct msg_data_s* msg, void* ext)
{
	if(msg->type == net_msg_data)
	{
		struct msg_data_s* sd_msg = ox_connection_sendmsg_claim(self, 1024);
		printf("recv %s \n", msg->data);
		memcpy(sd_msg->data, msg->data, msg->len);
		sd_msg->len = msg->len;
		ox_connection_send(self, sd_msg);
	}
}

int main()
{
	struct connection_s* client = ox_connection_new(1024, 1024, s_check, s_msg_handle, 0);
	struct msg_data_s* sd_msg = 0;
	ox_connection_send_connect(client, "127.0.0.1", 4100, 10000);
	sd_msg = ox_connection_sendmsg_claim(client, 1024);
	strcpy(sd_msg->data, "hello");
	sd_msg->len = strlen("hello")+1;
	ox_connection_send(client, sd_msg);

	while(1)
	{
		ox_connection_netpoll(client, 5);
		ox_connection_logicpoll(client, 5);
	}

	return 0;
}