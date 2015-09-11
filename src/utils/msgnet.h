#ifndef _MSGNET_H
#define _MSGNET_H

struct msgnet;
struct msg_module;

typedef void (*pfn_msg_handler)(struct msg_module* self, int msg_id, struct msg_module* source, void* data);

#define MSGNET_REGISTERMODULE_MSG 0
#define MSGNET_P2P_MSG 1

struct msg_module* msgmodule_create(pfn_msg_handler callback);

struct msgnet* msgnet_create(void);

/*  广播消息,所有注册感兴趣的事件为msgid的module都将处理此消息    */
void msgnet_broadcast(struct msgnet* self, int msgid, struct msg_module* source, void* data);

/*  向指定目标发送消息  */
void msgnet_sendto(int msgid, struct msg_module* source, struct msg_module* dest, void* data);

/*  help functions  */

void msgnet_send_register(struct msgnet* self, struct msg_module* module, int msgid);

#endif
