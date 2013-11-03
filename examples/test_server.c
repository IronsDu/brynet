#include <stdio.h>
#include <stdlib.h>

#include "thread.h"
#include "socketlibfunction.h"
#include "thread_reactor.h"

static int s_check(void* ud, const char* buffer, int len)
{
    return len;
}

static void    listen_thread(void* arg)
{
    sock client_fd = SOCKET_ERROR;
    struct sockaddr_in socketaddress;
    socklen_t size = sizeof(struct sockaddr);
    struct nr_mgr* mgr = (struct nr_mgr*)arg;

    sock listen_fd = ox_socket_listen(4100, 25);

    if(SOCKET_ERROR != listen_fd)
    {
        for(;;)
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
                ox_nrmgr_addfd(mgr, NULL, client_fd);
            }
        }

        ox_socket_close(listen_fd);
        listen_fd= SOCKET_ERROR;
    }
    else
    {
        printf("listen failed\n");
    }
}

static void msg_handle(struct nr_mgr* mgr, struct nrmgr_net_msg* msg)
{
    if(msg->type == nrmgr_net_msg_connect)
    {
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
        struct nrmgr_send_msg_data* sd_msg = ox_nrmgr_make_sendmsg(mgr, NULL, 1024);
		printf("recv %s \n", msg->data);
		memcpy(sd_msg->data, msg->data, msg->data_len);
        sd_msg->data_len = msg->data_len;
        /*  发送消息    */
        ox_nrmgr_sendmsg(mgr, sd_msg, msg->session);
    }
}

int main()
{
    struct nr_mgr* mgr = ox_create_nrmgr(1, 1024, 1024, s_check);
    ox_thread_new(listen_thread, mgr);

    while(true)
    {
        ox_nrmgr_logic_poll(mgr, msg_handle, 5);
    }

    return 0;
}