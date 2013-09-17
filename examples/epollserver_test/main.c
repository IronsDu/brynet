#include "thread.h"
#include "mutex.h"
#include "epollserver.h"
#include "server.h"
#include <stdio.h>
#include <signal.h>
#include <errno.h>

#define MAX_CLIENT_NUM 1024

const static int HTTP_PORT = 4002;
int index_array[MAX_CLIENT_NUM];
int index_num;
static struct mutex_s* m = NULL;

static struct mutex_s* all_recved_lock = NULL;
static int all_recved = 0;
struct server_s* server = NULL;

static void add_all_recved(int value)
{
        //mutex_lock(all_recved_lock);
        all_recved+=value;
        //mutex_unlock(all_recved_lock);
}

void insert_index(int index)
{
        mutex_lock(m);
        if(index_array[index] == -1)
        {
            index_array[index] = index;
            index_num++;
            printf("index_num is %d\n", index_num);
        }
        
        mutex_unlock(m);
}

void free_index(int index)
{
        mutex_lock(m);
        if(index_array[index] != -1)
        {
            index_array[index] = -1;
            index_num--;
            printf("index_num is %d\n", index_num);
        }
        else
        {
            printf("error\n");
            //getchar();
        }
        mutex_unlock(m);
}

void my_logic_on_enter_pt(struct server_s* self, int index)
{
     //printf("new client, index:%d\n", index);
     insert_index(index);
}
 
void my_logic_on_close_pt(struct server_s* self, int index)
{
	printf("client %d is closed in logic \n", index);
    free_index(index);
    server_close(server, index);
}
 
 struct packet_head_s
 {
     short len;
     short op;
     };
     
 int my_logic_on_recved_pt(struct server_s* self, int index, const char* buffer, int len)
 {
     //printf("recv data len:%d\n", len);
    if(len > 0)
    {
        int temp_index = 0;
        
        char temp[1024];
        
        struct packet_head_s* head = (struct packet_head_s*)temp;
        head->len = 10;
        
        server_send(self, index, temp, head->len);
/*
        for(; temp_index < index_num; ++temp_index)
        {
            server_send(self, index_array[temp_index], buffer, len);
        }
        * */
        
        //printf("接受到了数据\n");
         
        add_all_recved(1);
    }
    
    return len;
}

void test()
{
    server = epollserver_create(HTTP_PORT, MAX_CLIENT_NUM, 1024, 1024);
    printf("EAGAIN is %d\n", EAGAIN);
    printf("ECONNRESET is %d\n", ECONNRESET);
    
    server_start(server, my_logic_on_enter_pt, my_logic_on_close_pt, my_logic_on_recved_pt);
}

int main()
{
    int i = 0;
    int old_recv_len = 0;
    
    for(; i < MAX_CLIENT_NUM; ++i)
    {
        index_array[i] = -1;
    }
    
    index_num = 0;
    m  = mutex_new();
    all_recved_lock = mutex_new();
    test();
    
    while(true)
    {
        thread_sleep(1000);
        printf("all_recved is %dk/s\n", (all_recved-old_recv_len));
        old_recv_len = all_recved;
    }

    return 0;
}
