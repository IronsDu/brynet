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
#include "timeaction.h"
#include "list.h"

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

/*  最大累计数量  */
#define MAX_PENDING_NUM (20)
#define PENDING_POOL_SIZE (10024)

#define FLUSHTIMER_DELAY 50

struct nr_mgr
{
    struct net_reactor*     reactors;
    int                     reactor_num;
    int                     one_reactor_sessionnum; /*  单个reactor能够容纳多少个会话个数   */

    void*                   ud;
};

struct net_reactor;

/*  会话结构体   */
struct net_session_s
{
    struct double_link_s    packet_list;

    struct net_reactor*     reactor;

    const char*             bufs[MAX_SENDBUF_NUM];
    int                     lens[MAX_SENDBUF_NUM];
    int                     bufnum;

    bool                    active;                 /*  是否处于链接状态    */
    bool                    wait_flush;             /*  是否处于等待flush中    */
    int                     flush_id;               /*  刷新定时器的ID    */
    void*                   handle;                 /*  下层网络对象  */
    void*                   ud;                     /*  上层用户数据  */
};

struct net_reactor
{
    struct server_s*        server;
    struct nr_mgr*          mgr;

    int                     active_num;

    struct rwlist_s*        rwlist;

    struct rwlist_s*        free_sendmsg_list;  /*  网络层投递给逻辑层sendmsg_pool回收的逻辑消息包       */

    struct rwlist_s*        logic_msglist;      /*  网络层投递给逻辑层的消息包                           */
    struct rwlist_s*        free_logicmsg_list; /*  逻辑层投递给网络层logic_msgpool回收的网络消息包      */

    pfn_nrmgr_check_packet  check_packet;

    struct thread_s*        thread;

    struct timer_mgr_s*     flush_timer;

    struct list_s*          waitsend_list;
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
    RMT_ENTER,
    RMT_CLOSE,
    RMT_REQUEST_CLOSE,
};

struct send_msg
{
    struct nrmgr_send_msg_data* data;
    struct net_session_s*       session;
};

struct enter_data_s
{
    void*   ud;
    sock    fd;
};

struct close_data_s
{
    struct net_session_s* session;
};

struct rwlist_msg_data
{
    enum RWLIST_MSG_TYPE msg_type;
    union
    {
        struct send_msg send;
        struct enter_data_s enter;
        struct close_data_s close;
    }data;
};

static void
reactor_thread(void* arg);

static void 
reactor_logic_on_enter_callback(struct server_s* self, void* ud, void* handle);
 
static void
reactor_logic_on_close_callback(struct server_s* self, void* ud);
 
static int
reactor_logic_on_recved_callback(struct server_s* self, void* ud, const char* buffer, int len);

static void
session_sendfinish_callback(struct server_s* self, void* ud, int bytes);

static struct net_session_s*
net_session_malloc();

static void
session_destroy(struct net_reactor* reactor, struct net_session_s* session);

struct nr_mgr*
ox_create_nrmgr(
    int thread_num,
    int rbsize,
    pfn_nrmgr_check_packet check)
{
    struct nr_mgr* mgr = (struct nr_mgr*)malloc(sizeof(*mgr));
    int i = 0;

    mgr->reactors = (struct net_reactor*)malloc(sizeof(struct net_reactor) * thread_num);
    mgr->reactor_num = thread_num;

    for(; i < thread_num; ++i)
    {
        struct net_reactor* reactor = mgr->reactors+i;
        int j = 0;
        reactor->mgr = mgr;
        reactor->active_num = 0;
        #ifdef PLATFORM_LINUX
        reactor->server = epollserver_create(rbsize, sbsize, &mgr->reactors[i]);
        #else
        reactor->server = iocp_create(rbsize, 0, &mgr->reactors[i]);
        #endif
        
        reactor->rwlist = ox_rwlist_new(1024, sizeof(struct rwlist_msg_data) , DF_RWLIST_PENDING_NUM);

        reactor->free_sendmsg_list = ox_rwlist_new(DF_LIST_SIZE, sizeof(struct nrmgr_send_msg_data*), DF_RWLIST_PENDING_NUM*10);
        reactor->logic_msglist = ox_rwlist_new(DF_LIST_SIZE, sizeof(struct nrmgr_net_msg*), DF_RWLIST_PENDING_NUM);
        reactor->free_logicmsg_list = ox_rwlist_new(DF_LIST_SIZE, sizeof(struct nrmgr_net_msg**), DF_RWLIST_PENDING_NUM*10);
            
        reactor->check_packet = check;

        server_start(reactor->server, reactor_logic_on_enter_callback, reactor_logic_on_close_callback, reactor_logic_on_recved_callback, NULL, session_sendfinish_callback);
        reactor->flush_timer = ox_timer_mgr_new(1024);
        reactor->waitsend_list = ox_list_new(1024, sizeof(struct net_session_s*));
        reactor->thread = NULL;
        reactor->thread = ox_thread_new(reactor_thread, reactor);
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

        ox_rwlist_delete(reactor->rwlist);
        ox_rwlist_delete(reactor->free_sendmsg_list);
        ox_rwlist_delete(reactor->logic_msglist);
        ox_rwlist_delete(reactor->free_logicmsg_list);
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
        struct rwlist_msg_data msg;
        msg.msg_type = RMT_ENTER;
        msg.data.enter.ud = ud;
        msg.data.enter.fd = fd;

        ox_rwlist_push(nr->rwlist, &msg);
        ox_rwlist_force_flush(nr->rwlist);
    }
}

void 
ox_nrmgr_request_closesession(struct nr_mgr* mgr, struct net_session_s* session)
{
    struct net_reactor* reactor = session->reactor;
    struct rwlist_msg_data msg;
    msg.msg_type = RMT_REQUEST_CLOSE;
    msg.data.close.session = session;

    ox_rwlist_push(reactor->rwlist, &msg);
    ox_rwlist_force_flush(reactor->rwlist);
}

void
ox_nrmgr_closesession(struct nr_mgr* mgr, struct net_session_s* session)
{
    struct net_reactor* reactor = session->reactor;
    struct rwlist_msg_data msg;
    msg.msg_type = RMT_CLOSE;
    msg.data.close.session = session;

    ox_rwlist_push(reactor->rwlist, &msg);
    ox_rwlist_force_flush(reactor->rwlist);
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
    struct rwlist_s*    free_logicmsg_list = reactor->free_logicmsg_list;
    struct rwlist_s*    logic_msglist = reactor->logic_msglist;
    int64_t current_time = ox_getnowtime();
    const int64_t end_time = current_time+timeout;

    do
    {
        msg_pp = (struct nrmgr_net_msg**)ox_rwlist_pop(logic_msglist, end_time-current_time);
        if(msg_pp != NULL)
        {
            (callback)(mgr, *msg_pp);
            ox_rwlist_push(free_logicmsg_list, msg_pp);
        }

        current_time = ox_getnowtime();
    }while(end_time > current_time);
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
        ox_rwlist_flush(mgr->reactors[i].rwlist);
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
    ox_rwlist_push(session->reactor->rwlist, &msg);
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
    session->bufnum = 0;
    session->active = false;
    double_link_init(&session->packet_list);
    session->wait_flush = false;
    session->flush_id = -1;

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

    server_close(reactor->server, session->handle);
    session->active = false;
    session->wait_flush = false;
    if(session->flush_id != -1 && session->wait_flush)
    {
        ox_timer_mgr_del(reactor->flush_timer, session->flush_id);
        session->flush_id = -1;
    }

    free(session);
    reactor->active_num--;
}

static void
session_packet_flush(struct net_reactor* reactor, struct net_session_s* session)
{
    if(session->active && session->bufnum > 0)
    {
        int bytes = server_sendv(reactor->server, session->handle, session->bufs, session->lens, session->bufnum);
        if(bytes > 0)
        {
#ifdef PLATFORM_WINDOWS
            assert(false);
#endif
            session_sendfinish_callback(reactor->server, session->ud, bytes);
        }
        else if(bytes == -1)
        {
            /*  直接处理socket断开    */
            reactor_logic_on_close_callback(reactor->server, session);
        }
    }
}

static void
flushpacket_timerover(void* arg)
{
    struct net_session_s* session = (struct net_session_s*)arg;
    session->wait_flush = false;
    session_packet_flush(session->reactor, session);
}

static void 
insert_flush(struct net_reactor* reactor, struct net_session_s* session)
{
    if(!session->wait_flush)
    {
        session->flush_id = ox_timer_mgr_add(reactor->flush_timer, flushpacket_timerover, FLUSHTIMER_DELAY, session);
        session->wait_flush = true;
    }
}

static void
add_session_to_waitsendlist(struct net_reactor* reactor, struct net_session_s* session)
{
    if(!session->wait_flush)
    {
        session->wait_flush = true;
        ox_list_push_back(reactor->waitsend_list, &session);
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

            if(session->bufnum < MAX_SENDBUF_NUM)
            {
                session->bufs[session->bufnum] = pending->msg->data;
                session->lens[session->bufnum] = pending->left_len;
                ++(session->bufnum);

                if(false)
                {
                    insert_flush(reactor, session);
                }
                else
                {
                    /*	添加到活动send列表	*/
                    add_session_to_waitsendlist(reactor, session);
                }
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
    struct rwlist_s*    rwlist = reactor->rwlist;
    struct server_s* server = reactor->server;

    while((rwlist_msg = (struct rwlist_msg_data*)ox_rwlist_pop(rwlist, 0)) != NULL)
    {
        if(rwlist_msg->msg_type == RMT_ENTER)
        {
            struct net_session_s* session = net_session_malloc();
            if(session != NULL)
            {
                session->reactor = reactor;
                session->ud = rwlist_msg->data.enter.ud;

                if(!server_register(server, session, rwlist_msg->data.enter.fd))
                {
                    free(session);
                }
            }
            else
            {
                ox_socket_close(rwlist_msg->data.enter.fd);
            }
        }
        else if(rwlist_msg->msg_type == RMT_SENDMSG)
        {
            reactor_proc_sendmsg(reactor, rwlist_msg->data.send.session, rwlist_msg->data.send.data);
        }
        else if(rwlist_msg->msg_type == RMT_CLOSE)
        {
            session_destroy(reactor, rwlist_msg->data.close.session);
        }
        else if(rwlist_msg->msg_type == RMT_REQUEST_CLOSE)
        {
            if(rwlist_msg->data.close.session->active)
            {
                session_destroy(reactor, rwlist_msg->data.close.session);
            }
        }
    }
}

static void
reactor_proc_freelogiclist(struct net_reactor* reactor)
{
    struct nrmgr_net_msg** msg_p = NULL;
    struct nrmgr_net_msg* msg = NULL;
    struct rwlist_s*    free_logicmsg_list = reactor->free_logicmsg_list;

    while((msg_p = (struct nrmgr_net_msg**)ox_rwlist_pop(free_logicmsg_list, 0)) != NULL)
    {
        msg = *msg_p;
        free(msg);
    }
}

static void
reactor_thread(void* arg)
{
    struct net_reactor* reactor = (struct net_reactor*)arg;

    while(reactor->thread == NULL || ox_thread_isrun(reactor->thread))
    {
        reactor_proc_rwlist(reactor);
        reactor_proc_freelogiclist(reactor);

        ox_rwlist_flush(reactor->logic_msglist);
        /*  由用户给定网络库每次循环的超时时间   */
        server_pool(reactor->server, SOCKET_POLL_TIME);

        if(false)
        {
            ox_timer_mgr_schedule(reactor->flush_timer);
        }
        else
        {
            struct list_node_s* begin = ox_list_begin(reactor->waitsend_list);
            const struct list_node_s* end = ox_list_end(reactor->waitsend_list);

            while(begin != end)
            {
                struct net_session_s* session = *(struct net_session_s**)begin->data;
                session->wait_flush = false;
                begin = ox_list_erase(reactor->waitsend_list, begin);
                session_packet_flush(reactor, session);
            }
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

    assert(msg != NULL);

    if(msg != NULL)
    {
        session->active = true;
        reactor->active_num ++;
        ox_rwlist_push(reactor->logic_msglist, &msg);
    }
}

static void
reactor_logic_on_close_callback(struct server_s* self, void* ud)
{
    struct net_session_s* session = (struct net_session_s*)ud;
    struct net_reactor* reactor = (struct net_reactor*)server_getext(self);
    if(session->active)
    {
        struct nrmgr_net_msg* msg = make_logicmsg(reactor, nrmgr_net_msg_close, session, NULL, 0);
        assert(msg != NULL);

        if(msg != NULL)
        {
            /*  设置网络已断开 */
            session->active = false;
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
            assert(msg != NULL);
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

static void
session_sendfinish_callback(struct server_s* self, void* ud, int bytes)
{
    struct net_reactor* reactor = (struct net_reactor*)server_getext(self);
    struct net_session_s* session = (struct net_session_s*)ud;

    if(session->active)
    {
        {
            struct double_link_node_s* current = double_link_begin(&session->packet_list);
            struct double_link_node_s* end = double_link_end(&session->packet_list);

            while(current != end && bytes > 0)
            {
                struct pending_send_msg_s* node = (struct pending_send_msg_s*)current;
                struct double_link_node_s* next = current->next;
                if(node->left_len <= bytes)
                {
                    /*  当前packet已经发送完毕  */
                    double_link_erase(&session->packet_list, current);
                    bytes -= node->left_len;
                    ox_rwlist_push(reactor->free_sendmsg_list, &(node->msg));
                    free(node);
                }
                else
                {
                    node->left_len -= bytes;
                    break;
                }

                current = next;
            }
        }

        {
            /*  更新发送缓冲区 */
            struct double_link_node_s* current = double_link_begin((struct double_link_s*)&session->packet_list);
            struct double_link_node_s* end = double_link_end((struct double_link_s*)&session->packet_list);
            session->bufnum = 0;

            while(current != end && session->bufnum < MAX_SENDBUF_NUM)
            {
                struct pending_send_msg_s* node = (struct pending_send_msg_s*)current;
                struct double_link_node_s* next = current->next;
                session->bufs[session->bufnum] = node->msg->data+(node->msg->data_len-node->left_len);
                session->lens[session->bufnum] = node->left_len;
                session->bufnum++;

                current = next;
            }

            /*  没有完全发送完毕继续发送    */
            if(session->bufnum >= MAX_PENDING_NUM)
            {
                session_packet_flush(reactor, session);
            }
        }
    }
}

void ox_nrmgr_setuserdata(struct nr_mgr* mgr, void* ud)
{
    mgr->ud = ud;
}

void*  ox_nrmgr_getuserdata(struct nr_mgr* mgr)
{
    return mgr->ud;
}