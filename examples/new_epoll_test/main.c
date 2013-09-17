#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "thread.h"
#include "mutex.h"
#include "thread_reactor.h"
#include "socketlibtypes.h"
#include "socketlibfunction.h"

const char* msg_str[] = {"new client enter\n", "client closed\n", "client recvd data\n"};
/*  TODO::逻辑层使用完消息后可以push回网络层，进行回收再次使用  */

int all_recv_len = 0;

static void
my_msghandle(struct nr_mgr* mgr, struct net_msg* msg)
{
    //printf("logic thread recv netmsg, index:%d,msgtype:%s, len :%d\n", msg->index, msg_str[msg->type], msg->data_len);
    if(msg->type == net_msg_close)
    {
        nrmgr_closesession(mgr, msg->index);
    }
    else if(msg->type == net_msg_connect)
    {
        //nrmgr_sendmsg(mgr, make_sendmsg(mgr, "hello\n", 8), msg->index);
    }
    else if(msg->type == net_msg_data)
    {
        //printf("data:%s\n", msg->data);
    }
    
    if(msg->data_len > 0)
    {
        //struct send_msg_data* data = make_sendmsg(mgr, msg->data, msg->data_len);
        
        all_recv_len += msg->data_len;
        
        //nrmgr_sendmsg(mgr, data, msg->index);
    }
    
    nrmgr_free_netmsg(mgr, msg);
}

//逻辑层广播时，发送一个struct send_msg_data给网络层, ref+=1。网络层收到消息后直接发送，如果失败则为session分配一个私有pending_send_msg
//表示剩余数量，当pending_send_msg余数量为0时，将struct send_msg_data投递回逻辑层。逻辑层 ref-=1. 当ref为0时回收此msg

static void
printf_thread(void* arg)
{
    while(true)
    {
        int old = all_recv_len;
        thread_sleep(1000);
        //printf("all recv:%d M\n", (all_recv_len-old)/1024/1024);
    }
}

static void
listen_thread(void* arg)
{
    struct nr_mgr* mgr = (struct nr_mgr*)arg;

    sock client_fd = SOCKET_ERROR;
    struct sockaddr_in socketaddress;
    socklen_t size = sizeof(struct sockaddr);

    sock listen_fd = socket_listen(4002, 25);
    int total_num = 0;

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
                total_num++;
                //printf("accept %d, current total num : %d\n", client_fd, total_num);

                nrmgr_addfd(mgr, client_fd);
            }
        }

        socket_close(listen_fd);
        listen_fd = SOCKET_ERROR;
    }
    else
    {
        //printf("listen failed\n");
    }
    
    printf("exit\n");
    exit(0);
}

static int
check_packet(const char* buffer, int len)
{
    return len;
}

int main()
{
    struct nr_mgr* mgr = create_nrmgr(512, 1, 28, 1024, (pfn_logicmsg)my_msghandle, check_packet);
    thread_new(printf_thread, NULL);
    thread_new(listen_thread, mgr);
    
    while(true)
    {
        nr_mgr_logicmsg_handle(mgr);
        thread_sleep(1);
    }
    return 0;
}


