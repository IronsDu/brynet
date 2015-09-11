#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

#include "threadpool.h"
#include "msgnet.h"

#define MSG_ID_ARRAY_NUM 100

struct msg_module;
struct msgnet;

struct msg_module {
    struct msgnet* mgr;
    pfn_msg_handler callback;
};

struct msgmodule_node {
    struct msg_module* module;
    struct msgmodule_node* next;
};

struct msgnet {
    struct thread_pool_s* msg_proc_threads;
    /* TODO::目前采用的数据结构方式  */
    struct msgmodule_node nodes[MSG_ID_ARRAY_NUM];
};

struct msg_s {
    struct msgnet* mgr;
    int msg_id;
    void* data;
    struct msg_module* source;
};

/*  system data struct define */
struct reg_msg_data {
    struct msg_module* module;
    int msgid;
};

struct p2p_msg_data {
    struct msg_module* source;
    struct msg_module* dest;
    int msgid;
    void* data;
};

static void 
_msgnet_msg_handler(struct thread_pool_s* self, void* pmsg) {
    struct msg_s* msg = (struct msg_s*)pmsg;
    struct msgnet* mgr = msg->mgr;

    struct msgmodule_node* module_node = mgr->nodes+msg->msg_id;
    struct msg_module* module = NULL;

    while(module_node != NULL && (module = module_node->module) != NULL) {
        (module->callback)(module, msg->msg_id, msg->source, msg->data);
        module_node = module_node->next;
    }

    free(msg->data);
    free(msg);
}

static void 
_msgnet_insert_module(struct msgnet* mgr, struct msg_module* module, int msgid) {
    assert(msgid >= 0 && msgid < MSG_ID_ARRAY_NUM);

    module->mgr = mgr;

    if(mgr->nodes[msgid].module == NULL) {
        mgr->nodes[msgid].module = module;
        mgr->nodes[msgid].next = NULL;
    }
    else {
        struct msgmodule_node* module_node = (struct msgmodule_node*)malloc(sizeof(*module_node));
        module_node->module = module;
        module_node->next = mgr->nodes[msgid].next;
        mgr->nodes[msgid].next = module_node;
    }
}

static void 
_register_module_callback(struct msg_module* self, int msg_id, struct msg_module* source, void* data) {
    struct reg_msg_data* waitregmodule = (struct reg_msg_data*)data;
    if(waitregmodule != NULL) {
        /*  内置消息处理器之外的处理器不能注册REGISTER_MSG和P2P_MSG消息 */
        assert(waitregmodule->msgid != MSGNET_REGISTERMODULE_MSG && waitregmodule->msgid != MSGNET_P2P_MSG);
        if(waitregmodule->msgid != MSGNET_REGISTERMODULE_MSG && waitregmodule->msgid != MSGNET_P2P_MSG) {
            _msgnet_insert_module(self->mgr, waitregmodule->module, waitregmodule->msgid);
        } 
    }
}

static void 
_p2p_msg_callback(struct msg_module* self, int msg_id, struct msg_module* source, void* data) {
    struct p2p_msg_data* p2pmsg = (struct p2p_msg_data*)data;
    if(p2pmsg != NULL) {
        (p2pmsg->dest->callback)(p2pmsg->dest, p2pmsg->msgid, p2pmsg->source, p2pmsg->data);
        free(p2pmsg->data);
    }
}

struct msg_module* 
msgmodule_create(pfn_msg_handler callback) {
    struct msg_module* module = (struct msg_module*)malloc(sizeof(*module));
    module->callback = callback;
    return module;
}

struct msgnet* 
msgnet_create(void) {
    struct msgnet* ret = (struct msgnet*)malloc(sizeof(*ret));
    memset(ret, 0, sizeof(*ret));

    ret->msg_proc_threads = thread_pool_new(_msgnet_msg_handler, 2, 1000);
    thread_pool_start(ret->msg_proc_threads);

    {
        /*  预先创建module,注册其事件为REGISTER_MSG*/
        struct msg_module* module = msgmodule_create(_register_module_callback);
        _msgnet_insert_module(ret, module, MSGNET_REGISTERMODULE_MSG);
    }

    {
        /*  预先创建module,注册其事件为P2P_MSG*/
        struct msg_module* module = msgmodule_create(_p2p_msg_callback);
        _msgnet_insert_module(ret, module, MSGNET_P2P_MSG);
    }

    return ret;
}

void
msgnet_broadcast(struct msgnet* self, int msgid, struct msg_module* source, void* data) {
    /*    TODO::msg的分配和释放可以放入对象池中进行   */
    /*  self可以为NULL,此时使用source->mgr作为self,即broadcast时无需得到mgr,只需知道source即可  */

    struct msg_s* msg = (struct msg_s*)malloc(sizeof(*msg));
    msg->data = data;
    msg->mgr = (self == NULL ? (self=source->mgr): self);
    msg->msg_id = msgid;
    msg->source = source;

    thread_pool_pushmsg(self->msg_proc_threads, msg);
}

void 
msgnet_sendto(int msgid, struct msg_module* source, struct msg_module* dest, void* data) {
    struct p2p_msg_data* p2p_data = (struct p2p_msg_data*)malloc(sizeof(*p2p_data));
    p2p_data->source = source;
    p2p_data->dest = dest;
    p2p_data->msgid = msgid;
    p2p_data->data = data;
    msgnet_broadcast(dest->mgr, MSGNET_P2P_MSG, source, p2p_data);
}

void 
msgnet_send_register(struct msgnet* self, struct msg_module* module, int msgid) {
    struct reg_msg_data* reg_data = (struct reg_msg_data*)malloc(sizeof(*reg_data));
    reg_data->module = module;
    reg_data->msgid = msgid;
    msgnet_broadcast(self, MSGNET_REGISTERMODULE_MSG, module, reg_data);
}
