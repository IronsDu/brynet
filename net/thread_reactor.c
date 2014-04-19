#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>

#include "platform.h"
#include "thread.h"
#include "mutex.h"

#ifdef PLATFORM_LINUX
#include "epollserver.h"
#else
#include "iocp.h"
#endif

#include "server.h"
#include "socketlibfunction.h"
#include "rwlist.h"
#include "stack.h"
#include "systemlib.h"
#include "double_link.h"
#include "thread.h"

#include "thread_reactor.h"

/*  TODO::注意:如果少量客户端测试,此数字太小或许会导致某个线程一直获取不到消息(因为没有找过此上限而没有进行同步) */
#define DF_RWLIST_PENDING_NUM (5)

/*  TODO::默认大小设置    */
#define DF_LIST_SIZE (1024)

#define SOCKET_POLL_TIME (1)

struct nr_mgr
{
    struct net_reactor*     reactors;
    int                     reactor_num;

    void*                   ud;
};

struct net_reactor;

/*  会话结构体   */
struct net_session_s
{
    struct double_link_s    packet_list;

    struct net_reactor*     reactor;

    bool                    active;                 /*  是否处于链接状态    */
    bool                    wait_flush;             /*  是否处于等待flush中    */
    void*                   handle;                 /*  下层网络对象  */
    void*                   ud;                     /*  上层用户数据  */
    bool                    can_send;               /*  因为移除了buf数组，所以需要此变量，来避免每次都构造临时buf数组以及将session
                                                    放入带发送列表，如果没有移除buf数组成员，则无需此变量，而直接调用下层sendv，下层会判断
                                                    是否可写，也没有效率损耗*/
};

struct net_reactor
{
    struct server_s*        server;
    struct nr_mgr*          mgr;

    int                     active_num;

    struct rwlist_s*        free_sendmsg_list;  /*  网络层投递给逻辑层sendmsg_pool回收的逻辑消息包       */
    struct rwlist_s*        logic_msglist;      /*  网络层投递给逻辑层的消息包                           */

    struct rwlist_s*        enter_list;         /*  逻辑层投递给网络层新到链接的消息队列                  */
    struct rwlist_s*        fromlogic_rwlist;   /*  逻辑层投递给网络层的消息队列                          */

    pfn_nrmgr_check_packet  check_packet;

    struct thread_s*        thread;

    struct stack_s*         waitsend_list;
    struct stack_s*         waitclose_list;
};

/*  session待发的消息    */
struct pending_send_msg_s
{
    struct double_link_node_s   node;
    struct nrmgr_send_msg_data* msg;
    int                         left_len;   /*  剩余数据大小  */
};

enum RWLIST_MSG_TYPE
{
    RMT_SENDMSG,
    RMT_CLOSE,
    RMT_REQUEST_CLOSE,
    RMT_REQUEST_FREENETMSG,
};

struct rwlist_entermsg_data
{
    void*   ud;
    sock    fd;
};

struct rwlist_msg_data
{
    enum RWLIST_MSG_TYPE msg_type;

    union
    {
        struct
        {
            struct nrmgr_send_msg_data* data;
            struct net_session_s*       session;
        }send;

        struct
        {
            void*   ud;
            sock    fd;
        }enter;

        struct
        {
            struct net_session_s* session;
        }close;

        struct
        {
            struct nrmgr_net_msg*   msg;
        }free;
    }data;
};

static void
reactor_thread(void* arg);

static void
reactor_logic_on_enter_callback(struct server_s* self, void* ud, void* handle);

static void
reactor_logic_on_disconnection_callback(struct server_s* self, void* ud);

static int
reactor_logic_on_recved_callback(struct server_s* self, void* ud, const char* buffer, int len);

static void
session_sendfinish_callback(struct server_s* self, void* ud, int bytes);

static void
reactor_logic_on_close_completed(struct server_s* self, void* ud);

static void
reactor_freesendmsg_handle(struct net_reactor* reactor);

static struct net_session_s*
net_session_malloc();

static void
session_destroy(struct net_reactor* reactor, struct net_session_s* session);
static void
session_remove_sended_data(struct net_reactor* reactor, struct net_session_s* session, int sendbytes);

struct nr_mgr*
ox_create_nrmgr(
    int thread_num,
    int rbsize,
    pfn_nrmgr_check_packet check)
{
    struct nr_mgr* mgr = (struct nr_mgr*)malloc(sizeof(*mgr));

    if(mgr != NULL)
    {
        int i = 0;

        mgr->reactors = (struct net_reactor*)malloc(sizeof(struct net_reactor) * thread_num);
        mgr->reactor_num = thread_num;

        for(; i < thread_num; ++i)
        {
            struct net_reactor* reactor = mgr->reactors+i;

            reactor->mgr = mgr;
            reactor->active_num = 0;
#ifdef PLATFORM_LINUX
            reactor->server = epollserver_create(rbsize, 0, &mgr->reactors[i]);
#else
            reactor->server = iocp_create(rbsize, 0, &mgr->reactors[i]);
#endif

            reactor->free_sendmsg_list = ox_rwlist_new(DF_LIST_SIZE, sizeof(struct nrmgr_send_msg_data*), DF_RWLIST_PENDING_NUM*10);
            reactor->logic_msglist = ox_rwlist_new(DF_LIST_SIZE, sizeof(struct nrmgr_net_msg*), DF_RWLIST_PENDING_NUM);

            reactor->enter_list = ox_rwlist_new(DF_LIST_SIZE, sizeof(struct rwlist_entermsg_data), DF_RWLIST_PENDING_NUM*10);
            reactor->fromlogic_rwlist = ox_rwlist_new(1024, sizeof(struct rwlist_msg_data) , DF_RWLIST_PENDING_NUM);
            reactor->check_packet = check;

            server_start(reactor->server, reactor_logic_on_enter_callback, reactor_logic_on_disconnection_callback, reactor_logic_on_recved_callback, NULL, session_sendfinish_callback, reactor_logic_on_close_completed);
            reactor->waitsend_list = ox_stack_new(1024, sizeof(struct net_session_s*));
            reactor->waitclose_list = ox_stack_new(1024, sizeof(struct net_session_s*));
            reactor->thread = NULL;
            reactor->thread = ox_thread_new(reactor_thread, reactor);
        }
    }

    return mgr;
}

void
ox_nrmgr_delete(struct nr_mgr* self)
{
    int i = 0;
    for(; i < self->reactor_num; ++i)
    {
        ox_thread_delete(self->reactors[i].thread);
    }

    i = 0;
    for(; i < self->reactor_num; ++i)
    {
        struct net_reactor* reactor = self->reactors+i;

        ox_stack_delete(reactor->waitclose_list);
        ox_stack_delete(reactor->waitsend_list);

        ox_rwlist_force_flush(reactor->free_sendmsg_list);
        reactor_freesendmsg_handle(reactor);
        ox_rwlist_delete(reactor->free_sendmsg_list);
        ox_rwlist_delete(reactor->logic_msglist);
        ox_rwlist_delete(reactor->enter_list);

        {
            struct rwlist_s*    rwlist = reactor->fromlogic_rwlist;
            struct rwlist_msg_data* rwlist_msg = NULL;
            ox_rwlist_force_flush(reactor->fromlogic_rwlist);

            while((rwlist_msg = (struct rwlist_msg_data*)ox_rwlist_pop(rwlist, 0)) != NULL)
            {
                if(rwlist_msg->msg_type == RMT_SENDMSG)
                {
                    free(rwlist_msg->data.send.data);
                }
                else if(rwlist_msg->msg_type == RMT_CLOSE)
                {
                }
                else if(rwlist_msg->msg_type == RMT_REQUEST_CLOSE)
                {
                }
                else if(rwlist_msg->msg_type == RMT_REQUEST_FREENETMSG)
                {
                    free(rwlist_msg->data.free.msg);
                }
            }

            ox_rwlist_delete(reactor->fromlogic_rwlist);
        }
        

        server_stop(reactor->server);
    }

    free(self->reactors);

    free(self);
}

void
ox_nrmgr_addfd(struct nr_mgr* mgr, void* ud, int fd)
{
    struct net_reactor* nr = mgr->reactors+0;
    int i = 1;
    for(; i < mgr->reactor_num; ++i)
    {
        struct net_reactor* temp = mgr->reactors+i;
        if(temp->active_num < nr->active_num)
        {
            nr = temp;
        }
    }

    {
        struct rwlist_entermsg_data msg;
        msg.ud = ud;
        msg.fd = fd;

        ox_rwlist_push(nr->enter_list, &msg);
        ox_rwlist_force_flush(nr->enter_list);
    }
}

void
ox_nrmgr_request_closesession(struct nr_mgr* mgr, struct net_session_s* session)
{
    struct net_reactor* reactor = session->reactor;
    struct rwlist_msg_data msg;
    msg.msg_type = RMT_REQUEST_CLOSE;
    msg.data.close.session = session;

    ox_rwlist_push(reactor->fromlogic_rwlist, &msg);
    ox_rwlist_force_flush(reactor->fromlogic_rwlist);
}

void
ox_nrmgr_closesession(struct nr_mgr* mgr, struct net_session_s* session)
{
    struct net_reactor* reactor = session->reactor;
    struct rwlist_msg_data msg;
    msg.msg_type = RMT_CLOSE;
    msg.data.close.session = session;

    ox_rwlist_push(reactor->fromlogic_rwlist, &msg);
    ox_rwlist_force_flush(reactor->fromlogic_rwlist);
}

struct nrmgr_send_msg_data*
ox_nrmgr_make_sendmsg(struct nr_mgr* mgr, const char* src, int len)
{
    struct nrmgr_send_msg_data* ret = (struct nrmgr_send_msg_data*)malloc(len+sizeof(struct nrmgr_send_msg_data));

    if(ret != NULL)
    {
        ret->ref = 0;
        ret->data_len = 0;

        if(src != NULL)
        {
            memcpy(ret->data, src, len);
            ret->data_len = len;
        }
    }

    return ret;
}

static void
reactor_freesendmsg_handle(struct net_reactor* reactor)
{
    struct nrmgr_send_msg_data** msg = NULL;
    struct rwlist_s*    free_sendmsg_list = reactor->free_sendmsg_list;

    while((msg = (struct nrmgr_send_msg_data**)ox_rwlist_pop(free_sendmsg_list, 0)) != NULL)
    {
        struct nrmgr_send_msg_data* data = *msg;
        data->ref -= 1;
        assert(data->ref >= 0);
        if(data->ref == 0)
        {
            free(data);
        }
    }
}

static void
reactor_logicmsg_handle(struct net_reactor* reactor, pfn_nrmgr_logicmsg callback, int64_t timeout)
{
    struct nrmgr_net_msg** msg_pp = NULL;
    struct nr_mgr* mgr = reactor->mgr;
    struct rwlist_s*    logic_msglist = reactor->logic_msglist;
    int64_t current_time = ox_getnowtime();
    const int64_t end_time = current_time+timeout;
    struct rwlist_msg_data msg;
    struct rwlist_s*    rwlist = reactor->fromlogic_rwlist;
    msg.msg_type = RMT_REQUEST_FREENETMSG;

    while(true)
    {
        msg_pp = (struct nrmgr_net_msg**)ox_rwlist_pop(logic_msglist, end_time-current_time);
        current_time = ox_getnowtime();
        if(msg_pp != NULL)
        {
            (callback)(mgr, *msg_pp);

            msg.data.free.msg = *msg_pp;
            ox_rwlist_push(rwlist, &msg);
        }
        else
        {
            if(current_time >= end_time)
            {
                break;
            }
        }
    }
}

void
ox_nrmgr_logic_poll(struct nr_mgr* mgr, pfn_nrmgr_logicmsg msghandle, int64_t timeout)
{
    int i = 0;
    if(timeout <= 0)
    {
        timeout = 0;
    }
    else
    {
        timeout /= mgr->reactor_num;
    }

    for(; i < mgr->reactor_num; ++i)
    {
        reactor_logicmsg_handle(mgr->reactors+i, msghandle, timeout);
        ox_rwlist_flush(mgr->reactors[i].fromlogic_rwlist);
        reactor_freesendmsg_handle(mgr->reactors+i);
    }
}

void
ox_nrmgr_sendmsg(struct nr_mgr* mgr, struct nrmgr_send_msg_data* data, struct net_session_s* session)
{
    struct rwlist_msg_data msg;
    msg.msg_type = RMT_SENDMSG;
    msg.data.send.data = data;
    msg.data.send.session = session;

    data->ref += 1;
    ox_rwlist_push(session->reactor->fromlogic_rwlist, &msg);
}

static struct nrmgr_net_msg*
make_logicmsg(struct net_reactor* nr, enum nrmgr_net_msg_type type, struct net_session_s* session, const char* data, int data_len)
{
    struct nrmgr_net_msg* msg = (struct nrmgr_net_msg*)malloc(data_len+sizeof(struct nrmgr_net_msg));

    if(msg != NULL)
    {
        msg->type = type;
        msg->session = session;
        msg->ud = session->ud;
        msg->data_len = 0;

        if(data != NULL)
        {
            memcpy(msg->data, data, data_len);
            msg->data_len = data_len;
        }
    }

    return msg;
}

static struct net_session_s*
net_session_malloc()
{
    struct net_session_s* session = (struct net_session_s*)malloc(sizeof(*session));
    if(session != NULL)
    {
        session->active = true;
        double_link_init(&session->packet_list);
        session->wait_flush = false;
        session->can_send = true;
    }

    return session;
}

static void
session_destroy(struct net_reactor* reactor, struct net_session_s* session)
{
    struct double_link_node_s* current = double_link_begin(&session->packet_list);
    struct double_link_node_s* end = double_link_end(&session->packet_list);

    while(current != end)
    {
        struct pending_send_msg_s* node = (struct pending_send_msg_s*)current;
        struct double_link_node_s* next = current->next;
        double_link_erase(&session->packet_list, current);
        ox_rwlist_push(reactor->free_sendmsg_list, &(node->msg));
        free(node);
        current = next;
    }

    double_link_init(&session->packet_list);

    session->active = false;
    session->wait_flush = false;
    free(session);
    reactor->active_num--;
}

static void
session_packet_flush(struct net_reactor* reactor, struct net_session_s* session)
{
    /*  构造writev缓冲区数组 */
    const char*             bufs[MAX_SENDBUF_NUM];
    int                     lens[MAX_SENDBUF_NUM];
    int                     bufnum = 0;
    int                     total_len = 0;

    struct double_link_node_s* current = double_link_begin((struct double_link_s*)&session->packet_list);
    struct double_link_node_s* end = double_link_end((struct double_link_s*)&session->packet_list);

    while (current != end && bufnum < MAX_SENDBUF_NUM)
    {
        struct pending_send_msg_s* node = (struct pending_send_msg_s*)current;
        struct double_link_node_s* next = current->next;
        bufs[bufnum] = node->msg->data + (node->msg->data_len - node->left_len);
        lens[bufnum] = node->left_len;
        bufnum++;
        total_len += node->left_len;

        current = next;
    }

    if (session->active && bufnum > 0)
    {
        int bytes = server_sendv(reactor->server, session->handle, bufs, lens, bufnum);
        if(bytes > 0)
        {
#ifdef PLATFORM_WINDOWS
            assert(false);
#endif

#ifdef PLATFORM_LINUX
            /*  linux下是立即完成，所以立即移除以发送数据 */
            session->can_send = bytes == total_len;
            session_remove_sended_data(reactor, session, bytes);
#endif
        }
        else if(bytes == -1)
        {
            /*  直接处理socket断开    */
            session->can_send = false;
            reactor_logic_on_disconnection_callback(reactor->server, session);
        }
        else
        {
            /*  win下socket只要没断开，则都返回0， linux下当写入失败时返回0  */
            session->can_send = false;
        }
    }
}

static void
add_session_to_waitsendlist(struct net_reactor* reactor, struct net_session_s* session)
{
    if(!session->wait_flush)
    {
        session->wait_flush = true;
        ox_stack_push(reactor->waitsend_list, &session);
    }
}

static void
reactor_proc_sendmsg(struct net_reactor* reactor, struct net_session_s* session, struct nrmgr_send_msg_data* data)
{
    if(session->active)
    {
        struct pending_send_msg_s* pending = (struct pending_send_msg_s*)malloc(sizeof *pending);

        if(pending != NULL)
        {
            /*  直接放入链表  */
            pending->node.next = pending->node.prior = NULL;
            pending->msg = data;
            pending->left_len = data->data_len;
            double_link_push_back(&(session->packet_list), (struct double_link_node_s*)pending);

            /*	添加到待send列表	*/
            if (session->can_send)
            {
                add_session_to_waitsendlist(reactor, session);
            }
        }
        else
        {
            ox_rwlist_push(reactor->free_sendmsg_list, &data);
        }
    }
    else
    {
        ox_rwlist_push(reactor->free_sendmsg_list, &data);
    }
}

static void
reactor_proc_rwlist(struct net_reactor* reactor)
{
    struct rwlist_msg_data* rwlist_msg = NULL;
    struct rwlist_s*    rwlist = reactor->fromlogic_rwlist;

    while((rwlist_msg = (struct rwlist_msg_data*)ox_rwlist_pop(rwlist, 0)) != NULL)
    {
        if(rwlist_msg->msg_type == RMT_SENDMSG)
        {
            reactor_proc_sendmsg(reactor, rwlist_msg->data.send.session, rwlist_msg->data.send.data);
        }
        else if(rwlist_msg->msg_type == RMT_CLOSE)
        {
            if (!rwlist_msg->data.close.session->active)
            {
                ox_stack_push(reactor->waitclose_list, &rwlist_msg->data.close.session);
            }
        }
        else if(rwlist_msg->msg_type == RMT_REQUEST_CLOSE)
        {
            if(rwlist_msg->data.close.session->active)
            {
                ox_stack_push(reactor->waitclose_list, &rwlist_msg->data.close.session);
            }
        }
        else if(rwlist_msg->msg_type == RMT_REQUEST_FREENETMSG)
        {
            free(rwlist_msg->data.free.msg);
            rwlist_msg->data.free.msg = NULL;
        }
    }
}

static void
reactor_proc_enterlist(struct net_reactor* reactor)
{
    struct rwlist_entermsg_data* enter_msg = NULL;
    struct rwlist_s*    enter_list = reactor->enter_list;
    struct server_s* server = reactor->server;

    while((enter_msg = (struct rwlist_entermsg_data*)ox_rwlist_pop(enter_list, 0)) != NULL)
    {
        struct net_session_s* session = net_session_malloc();
        if(session != NULL)
        {
            session->reactor = reactor;
            session->ud = enter_msg->ud;

            if(!server_register(server, session, enter_msg->fd))
            {
                printf("register failed , free session \n");
                free(session);
            }
        }
        else
        {
            ox_socket_close(enter_msg->fd);
        }
    }
}

static void
reactor_thread(void* arg)
{
    struct net_reactor* reactor = (struct net_reactor*)arg;

    while(reactor->thread == NULL || ox_thread_isrun(reactor->thread))
    {
        /*  由用户给定网络库每次循环的超时时间   */
        server_pool(reactor->server, SOCKET_POLL_TIME);
        ox_rwlist_flush(reactor->logic_msglist);

        reactor_proc_rwlist(reactor);
        reactor_proc_enterlist(reactor);

        {
            struct stack_s* waitsend_list = reactor->waitsend_list;
            char* data = NULL;
            while((data = ox_stack_popfront(waitsend_list)) != NULL)
            {
                struct net_session_s* session = *(struct net_session_s**)data;
                session->wait_flush = false;

                session_packet_flush(reactor, session);
            }
        }

        {
            struct stack_s* waitclose_list = reactor->waitclose_list;
            char* data = NULL;
            while((data = ox_stack_popfront(waitclose_list)) != NULL)
            {
                struct net_session_s* session = *(struct net_session_s**)data;
                /*  请求底层释放资源，并会触发释放完成    */
                server_close(reactor->server, session->handle);
            }

            /*  在处理了waitsend_list之后，使用专门的list来储存待关闭的session，以防reactor_proc_rwlist将session放入sendlist之后，然后free了session    */
        }
    }
}

static void
reactor_logic_on_enter_callback(struct server_s* self, void* ud, void* handle)
{
    struct net_session_s* session = (struct net_session_s*)ud;
    struct net_reactor* reactor = (struct net_reactor*)server_getext(self);
    struct nrmgr_net_msg* msg = make_logicmsg(reactor, nrmgr_net_msg_connect, session, NULL, 0);
    session->handle = handle;

    if(msg != NULL)
    {
        session->active = true;
        reactor->active_num ++;
        ox_rwlist_push(reactor->logic_msglist, &msg);
    }
}

static void
reactor_logic_on_disconnection_callback(struct server_s* self, void* ud)
{
    struct net_session_s* session = (struct net_session_s*)ud;
    struct net_reactor* reactor = (struct net_reactor*)server_getext(self);
    if(session->active)
    {
        struct nrmgr_net_msg* msg = make_logicmsg(reactor, nrmgr_net_msg_close, session, NULL, 0);
        /*  设置网络已断开，并告诉上层 */
        session->active = false;

        if(msg != NULL)
        {
            ox_rwlist_push(reactor->logic_msglist, &msg);
        }
    }
}

static int
reactor_logic_on_recved_callback(struct server_s* self, void* ud, const char* buffer, int len)
{
    struct net_reactor* reactor = (struct net_reactor*)server_getext(self);
    struct net_session_s* session = (struct net_session_s*)ud;

    int proc_len = 0;
    int left_len = len;
    const char* check_buffer = buffer;
    struct rwlist_s*    logic_msglist = reactor->logic_msglist;

    while(left_len > 0)
    {
        int check_len = (reactor->check_packet)(session->ud, check_buffer, left_len);
        if(check_len > 0)
        {
            struct nrmgr_net_msg* msg = make_logicmsg(reactor, nrmgr_net_msg_data, session, check_buffer, check_len);
            if(msg != NULL)
            {
                ox_rwlist_push(logic_msglist, &msg);
            }

            check_buffer += check_len;
            left_len -= check_len;
            proc_len += check_len;
        }
        else
        {
            break;
        }
    }

    return proc_len;
}

/*  移除已经发送的数据   */
static void
session_remove_sended_data(struct net_reactor* reactor, struct net_session_s* session, int sendbytes)
{
    struct double_link_node_s* current = double_link_begin(&session->packet_list);
    struct double_link_node_s* end = double_link_end(&session->packet_list);

    while (current != end && sendbytes > 0)
    {
        struct pending_send_msg_s* node = (struct pending_send_msg_s*)current;
        struct double_link_node_s* next = current->next;
        if (node->left_len <= sendbytes)
        {
            /*  当前packet已经发送完毕  */
            double_link_erase(&session->packet_list, current);
            sendbytes -= node->left_len;
            ox_rwlist_push(reactor->free_sendmsg_list, &(node->msg));
            free(node);
        }
        else
        {
            node->left_len -= sendbytes;
            break;
        }

        current = next;
    }
}

static void
session_sendfinish_callback(struct server_s* self, void* ud, int bytes)
{
    struct net_reactor* reactor = (struct net_reactor*)server_getext(self);
    struct net_session_s* session = (struct net_session_s*)ud;

    if(session->active)
    {
        session->can_send = true;
        session_remove_sended_data(reactor, session, bytes);
        /*  不管是linux下收到的可写通知(bytes为0），还是iocp收到的发送完成通知，均将此session放入待发送队列 */
        add_session_to_waitsendlist(reactor, session);
    }
}

static void
reactor_logic_on_close_completed(struct server_s* self, void* ud)
{
    struct net_reactor* reactor = (struct net_reactor*)server_getext(self);
    struct net_session_s* session = (struct net_session_s*)ud;
    session_destroy(reactor, session);
}

void ox_nrmgr_setuserdata(struct nr_mgr* mgr, void* ud)
{
    mgr->ud = ud;
}

void*  ox_nrmgr_getuserdata(struct nr_mgr* mgr)
{
    return mgr->ud;
}
