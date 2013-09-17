#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "thread.h"
#include "msgnet.h"

#define TEST_MSG_ID 2

/*  以下消息用于P2P发送,无需注册到管理器    */
#define TEST_MSG_ID_LOGIN 3
#define TEST_MSG_ID_LOGIN_REPLY 4

void test_callback(struct msg_module* module, int msg_id, struct msg_module* source, void* data) {
    if(msg_id == TEST_MSG_ID) {
        char* name = (char*)data;
        printf("my test, msgid :%d, name is : %s\n", msg_id, name);
    } 
    else if(msg_id == TEST_MSG_ID_LOGIN_REPLY) {
        bool* result = (bool*)data;
        printf("my test, msgid : %d, login result is : %d\n", msg_id, *result);
    }
}

void test_callback1(struct msg_module* module, int msg_id, struct msg_module* source, void* data) {
    if(msg_id == TEST_MSG_ID) {
        char* name = (char*)data;
        printf("my test 1, msgid :%d, name is : %s\n", msg_id, name);
    }
    else if(msg_id == TEST_MSG_ID_LOGIN) {
        char* name = (char*)data;
        bool* result = (bool*)malloc(sizeof(*result));
        *result = true;
        printf("recv login request , name is : %s , in test 2\n", name);
        msgnet_sendto(TEST_MSG_ID_LOGIN_REPLY, module, source, result);
    }
}

int main(void) {
    struct msg_module* module = msgmodule_create(test_callback);
    struct msg_module* module1 = msgmodule_create(test_callback1);

    struct msgnet* mgr = msgnet_create();
    char* string = strdup("dzw");
    char* string1 = strdup("dzw1");

    msgnet_send_register(mgr, module, TEST_MSG_ID);
    msgnet_send_register(mgr, module1, TEST_MSG_ID);
    msgnet_send_register(mgr, module1, TEST_MSG_ID_LOGIN);
  
    thread_sleep(1000);                     /*  TODO::先让注册module的消息被处理    */
  
    msgnet_broadcast(mgr, TEST_MSG_ID, NULL, string);  /*    发送消息    */

    thread_sleep(1000);
    msgnet_sendto(TEST_MSG_ID_LOGIN, module, module1, string1);
    getchar();
    return 0;
}
