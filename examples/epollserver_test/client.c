#include <stdio.h>
#include "socketlibfunction.h"
#include "thread.h"

#define ONE_THREAD_FD_NUM (512)
#define NET_PORT 4002
#define THREAD_NUM 2

void send_thread(void* arg)
{
    sock s_arr[ONE_THREAD_FD_NUM] = {-1};
    int i = 0;
    
    for(; i < ONE_THREAD_FD_NUM; ++i)
    {
        s_arr[i] = socket_connect("127.0.0.1", NET_PORT);
        printf("当前链接索引：%d\n", i);
        socket_nonblock(s_arr[i]);
    }
    
    for(;;)
    {
        i = 0;
        for(; i < ONE_THREAD_FD_NUM; ++i)
        {
            sock s = s_arr[i];
            if(-1 == s) continue;
            
            int len = 0;
            /*
            len = socket_send(s, "dzwdzwdzwdzwdzwdzwdzwdzwdzwdzwdzw\
            dzwdzwdzwdzwdzwdzwdzwdzwdzwdzwdzw\
            dzwdzwdzwdzwdzwdzwdzwdzwdzwdzwdzw\
            dzwdzwdzwdzwdzwdzwdzwdzwdzwdzwdzw\
            dzwdzwdzwdzwdzwdzwdzwdzwdzwdzwdzwdzwdzwdzwdzwdzwdzwdzwdzwdzwdzwdzwdzwdzwdzwdzwdzwdzwdzwdzwdzwdzwdzwdzwdzwdzwdzwdzwdzwdzwdzwdzwdzwdzw", 200);
*/
            //printf("send len is %d\n", len);
        }
        
        thread_sleep(1);
    }
}

int main()
{
    int i = 0;
    struct thread_s* thread = NULL;
    for(; i < THREAD_NUM; ++i)
    {
        thread = thread_new(send_thread, NULL);
    }
    //send_thread(NULL);
    getchar();
    return 0;
}
