#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "thread.h"
#include "mutex.h"
#include "thread_reactor.h"
#include "socketlibtypes.h"
#include "socketlibfunction.h"
#include "packet.h"

const char* msg_str[] = {"new client enter\n", "client closed\n", "client recvd data\n"};
/*  TODO::逻辑层使用完消息后可以push回网络层，进行回收再次使用  */

int all_recv_len = 0;
#define TEST_ECHO ("<html>hello</html>")

static void
my_msghandle(struct nr_mgr* mgr, struct nrmgr_net_msg* msg)
{
    //printf("logic thread recv netmsg, index:%d,msgtype:%s, len :%d\n", msg->index, msg_str[msg->type], msg->data_len);
    if(msg->type == nrmgr_net_msg_close)
    {
        nrmgr_closesession(mgr, msg->index);
    }
    else if(msg->type == nrmgr_net_msg_connect)
    {
        //nrmgr_sendmsg(mgr, make_sendmsg(mgr, "hello\n", 8), msg->index);
    }
    else if(msg->type == nrmgr_net_msg_data)
    {
        //printf("data:%s\n", msg->data);
        struct nrmgr_send_msg_data* send_msg = nrmgr_make_type_sendmsg(mgr, NULL, 128, nrmgr_packet_pool_tiny);

        int msg_len = strlen(TEST_ECHO)+1;
        struct packet_head_s* head = (struct packet_head_s*)send_msg->data;

        memcpy(send_msg->data+sizeof(*head), TEST_ECHO, msg_len);
        send_msg->data_len = msg_len+sizeof(*head);
        head->len = send_msg->data_len;

        nrmgr_sendmsg(mgr, send_msg, msg->index);

        all_recv_len+= 1;
    }
    //
    //if(msg->data_len > 0)
    //{
    //    struct send_msg_data* data = make_sendmsg(mgr, msg->data, msg->data_len);
    //    
    //    all_recv_len += msg->data_len;
    //    
    //    nrmgr_sendmsg(mgr, data, msg->index);
    //}
    
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
        printf("all recv:%d B\n", (all_recv_len-old));
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
}

static int
check_packet(const char* buffer, int len)
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

#define SLEEP_COUNT (100000)

int main()
{
    struct nr_mgr* mgr = create_nrmgr(1024, 1, 4096, 4096, (pfn_nrmgr_logicmsg)my_msghandle, check_packet);
    int i = 0;
    thread_new(printf_thread, NULL);
    thread_new(listen_thread, mgr);
    
    while(true)
    {
        nrmgr_logicmsg_handle(mgr);
        if(i++ == SLEEP_COUNT)
        {
            thread_sleep(1);
            i = 0;
        }
    }
    return 0;
}


