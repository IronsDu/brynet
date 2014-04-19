
#define CRTDBG_MAP_ALLOC    
#include <stdlib.h>    
#include <crtdbg.h>

#include <stdio.h>
#include <stdlib.h>


#include "thread.h"
#include "socketlibfunction.h"
#include "thread_reactor.h"
#include "systemlib.h"

#include "pingpong_def.h"


static int totaol_recv = 0;

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

struct thread_s* lt;

static void    listen_thread(void* arg)
{
    sock client_fd = SOCKET_ERROR;
    struct sockaddr_in socketaddress;
    socklen_t size = sizeof(struct sockaddr);
    struct nr_mgr* mgr = (struct nr_mgr*)arg;

    sock listen_fd = ox_socket_listen(SERVER_PORT, 25);

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
                if(ox_thread_isrun(lt))
                {
                    //printf("accept client_fd : %d \n", client_fd);
                    ox_nrmgr_addfd(mgr, NULL, client_fd);
                }
                else
                {
                    ox_socket_close(client_fd);
                }
            }

            if(!ox_thread_isrun(lt))
            {
                break;
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
    static int current_num = 0;

    if(msg->type == nrmgr_net_msg_connect)
    {
        current_num++;
        printf("connection : %p enter , current num : %d \n", msg->session, current_num);
        //ox_nrmgr_request_closesession(mgr, msg->session);
    }
    else if(msg->type == nrmgr_net_msg_close)
    {
        current_num--;
        printf("connection :%p close , close session, current num : %d \n", msg->session, current_num);
        ox_nrmgr_closesession(mgr, msg->session);
    }
    else if(msg->type == nrmgr_net_msg_data)
    {
        /*  申请发送消息  */
		struct nrmgr_send_msg_data* sd_msg = ox_nrmgr_make_sendmsg(mgr, NULL, msg->data_len);
		totaol_recv += msg->data_len;
		//memcpy(sd_msg->data, msg->data, msg->data_len);
        sd_msg->data_len = msg->data_len;
        /*  发送消息    */
        ox_nrmgr_sendmsg(mgr, sd_msg, msg->session);
    }
}

int kbhit(void)
{
#if defined(_MSC_VER)
    return _kbhit();
#else
    struct termios oldt, newt;
    int ch;
    int oldf;

    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

    ch = getchar();

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);

    if(ch != EOF)
    {
        ungetc(ch, stdin);
        return 1;
    }

    return 0;
#endif
}

int main()
{
    struct nr_mgr* mgr = ox_create_nrmgr(1, PACKET_LEN*2, s_check);
	int old = ox_getnowtime();
    int64_t now = ox_getnowtime();
    lt = ox_thread_new(listen_thread, mgr);
    
    while(true)
    {
        ox_nrmgr_logic_poll(mgr, msg_handle, 5);
		{
			int now = ox_getnowtime();
			if((now - old) >= 1000)
			{
                printf("recv %d K/s \n", totaol_recv/1024);
				old = now;
				totaol_recv = 0;
			}
		}

        if(kbhit())
        {
            char c = getchar();
            if(c == 'q')
            {
                break;
            }
        }
    }

    ox_socket_connect("127.0.0.1", SERVER_PORT);
    ox_thread_delete(lt);
    ox_nrmgr_delete(mgr);

    _CrtDumpMemoryLeaks();

    return 0;
}
