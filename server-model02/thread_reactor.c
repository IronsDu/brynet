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

#ifdef PLATFORM_LINUX
#include "epollserver.h"
#else
#include "iocp.h"
#endif

#include "server.h"
#include "socketlibfunction.h"
#include "rwlist.h"
#include "stack.h"
#include "typepool.h"
#include "multipool.h"
#include "systemlib.h"
#include "double_link.h"
#include "thread.h"
#include "typepool.h"

#include "thread_reactor.h"

struct nr_mgr
{
    struct net_reactor*     reactors;
    int                     reactor_num;
    int                     one_reactor_sessionnum; /*  单个reactor能够容纳多少个会话个数   */

    struct multi_pool_s*    sendmsg_pools;          /*  逻辑层要给网络层投递逻辑消息所用的缓存池    */

    void*                   ud;
};

#define MAX_PENDING_MSG (10)

/*  TODO::注意:如果少量客户端测试,此数字太小或许会导致某个线程一直获取不到消息(因为没有找过此上限而没有进行同步) */
#define DF_RWLIST_PENDING_NUM (5)

/*  TODO::默认大小设置    */
#define DF_LIST_SIZE (1024)

#define SOCKET_POLL_TIME (1)

/*  最大累计数量  */
#define MAX_PENDING_NUM (20)
#define PENDING_POOL_SIZE (10024)

#define FLUSHTIMER_DELAY 50

/*  session待发的消息    */
struct pending_send_msg_s
{
    struct double_link_node_s   node;
    struct nrmgr_send_msg_data* msg;
    int                         left_len;   /*  剩余数据大小  */
};

struct net_reactor;

/*  会话结构体   */
struct net_session_s
{
    struct double_link_s    packet_list;

    struct net_reactor*     reactor;
    int                     index;

    const char*             bufs[MAX_SENDBUF_NUM];
    int                     lens[MAX_SENDBUF_NUM];
    int                     bufnum;

    bool                    active;                 /*  是否处于链接状态    */
    bool                    wait_flush;             /*  是否处于等待flush中    */  
};

struct net_reactor
{
    struct server_s*        server;
    struct nr_mgr*          mgr;

    int                     index;
    int                     session_startindex;

    int                     active_num;

    struct net_session_s*   sessions;

    struct type_pool_s*     pending_packet_pool;

    struct rwlist_s*        enter_list;
    struct rwlist_s*        wait_close_list;
    struct rwlist_s*        send_list;

    struct rwlist_s*        free_sendmsg_list;  /*  网络层投递给逻辑层sendmsg_pool回收的逻辑消息包       */

    struct rwlist_s*        logic_msglist;      /*  网络层投递给逻辑层的消息包                           */
    struct rwlist_s*        free_logicmsg_list; /*  逻辑层投递给网络层logic_msgpool回收的网络消息包      */
    struct multi_pool_s*    logic_msgpools;     /*  网络层要给逻辑层投递网络消息所用的缓存池            */

    pfn_nrmgr_check_packet  check_packet;

    struct thread_s*        thread;

    int client_max;

    struct timer_mgr_s*     flush_timer;
};

struct send_msg
{
    struct nrmgr_send_msg_data* data;
    int                         index;
};

static void
reactor_thread(void* arg);

static void 
my_logic_on_enter_pt(struct server_s* self, int index);
 
static void
my_logic_on_close_pt(struct server_s* self, int index);
 
static int
my_logic_on_recved_pt(struct server_s* self, int index, const char* buffer, int len);

static void
reactor_pushenter(struct net_reactor* reactor, sock fd);

static void
net_session_init(struct net_session_s* session)
{
    session->bufnum = 0;
    session->active = false;
    double_link_init(&session->packet_list);
    session->wait_flush = false;
}

static void
session_sendfinish_callback(struct server_s* self, int index, int bytes);

struct nr_mgr*
ox_create_nrmgr(
    int num, 
    int thread_num,
    int rbsize,
    int sbsize,
    pfn_nrmgr_check_packet check,
    struct nr_server_msgpool_config config)
{
    struct nr_mgr* mgr = (struct nr_mgr*)malloc(sizeof(*mgr));
    int one_thread_num = (num/thread_num);
    int i = 0;

    mgr->reactors = (struct net_reactor*)malloc(sizeof(struct net_reactor) * thread_num);
    mgr->reactor_num = thread_num;

    for(; i < thread_num; ++i)
    {
        struct net_reactor* reactor = mgr->reactors+i;
        int j = 0;
        reactor->mgr = mgr;
        reactor->index = i;
        reactor->session_startindex = one_thread_num*i;
        reactor->active_num = 0;
        reactor->sessions = (struct net_session_s*)malloc(sizeof(struct net_session_s)*one_thread_num);
        {
            int i = 0;
            for(; i < one_thread_num; ++i)
            {
                net_session_init(reactor->sessions+i);
                reactor->sessions[i].reactor = reactor;
                reactor->sessions[i].index = i;
            }
        }

        #ifdef PLATFORM_LINUX
        reactor->server = epollserver_create(one_thread_num, rbsize, sbsize, &mgr->reactors[i]);
        #else
        reactor->server = iocp_create(one_thread_num, rbsize, sbsize, &mgr->reactors[i]);
        #endif
        
        reactor->enter_list = ox_rwlist_new(one_thread_num, sizeof(sock) , DF_RWLIST_PENDING_NUM);
        reactor->wait_close_list = ox_rwlist_new(one_thread_num, sizeof(int), DF_RWLIST_PENDING_NUM);

        reactor->send_list = ox_rwlist_new(DF_LIST_SIZE, sizeof(struct send_msg), DF_RWLIST_PENDING_NUM);
        reactor->free_sendmsg_list = ox_rwlist_new(DF_LIST_SIZE, sizeof(struct nrmgr_send_msg_data*), DF_RWLIST_PENDING_NUM*10);
        reactor->logic_msglist = ox_rwlist_new(DF_LIST_SIZE, sizeof(struct nrmgr_net_msg*), DF_RWLIST_PENDING_NUM);
        reactor->free_logicmsg_list = ox_rwlist_new(DF_LIST_SIZE, sizeof(struct nrmgr_net_msg**), DF_RWLIST_PENDING_NUM*10);
        reactor->logic_msgpools = ox_multi_pool_new(config.logicmsg_pool_num, config.logicmsg_pool_len, config.logicmsg_pool_typemax, sizeof(struct nrmgr_net_msg));
            
        reactor->check_packet = check;

        server_start(reactor->server, my_logic_on_enter_pt, my_logic_on_close_pt, my_logic_on_recved_pt, NULL, session_sendfinish_callback);

        reactor->client_max = one_thread_num;

        reactor->pending_packet_pool = ox_type_pool_new(PENDING_POOL_SIZE, sizeof(struct pending_send_msg_s));
        reactor->flush_timer = ox_timer_mgr_new(one_thread_num);

        reactor->thread = NULL;
        reactor->thread = ox_thread_new(reactor_thread, reactor);
    }

    mgr->one_reactor_sessionnum = one_thread_num;
    mgr->sendmsg_pools = ox_multi_pool_new(config.sendmsg_pool_num, config.sendmsg_pool_len, config.sendmsg_pool_typemax, sizeof(struct nrmgr_send_msg_data));

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

        ox_rwlist_delete(reactor->enter_list);
        ox_rwlist_delete(reactor->wait_close_list);
        ox_rwlist_delete(reactor->send_list);
        ox_rwlist_delete(reactor->free_sendmsg_list);
        ox_rwlist_delete(reactor->logic_msglist);
        ox_rwlist_delete(reactor->free_logicmsg_list);
        ox_type_pool_delete(reactor->pending_packet_pool);
        ox_multi_pool_delete(reactor->logic_msgpools);
        free(reactor->sessions);
        server_stop(reactor->server);
    }

    free(self->reactors);
    ox_multi_pool_delete(self->sendmsg_pools);

    free(self);
}

static void
reactor_pushclose(struct net_reactor* reactor, int index)
{
    ox_rwlist_push(reactor->wait_close_list, &index);
    ox_rwlist_force_flush(reactor->wait_close_list);
}

void
ox_nrmgr_addfd(struct nr_mgr* mgr, int fd)
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
    reactor_pushenter(nr, fd);
}

void
ox_nrmgr_closesession(struct nr_mgr* mgr, int index)
{
    struct net_reactor* reactor = mgr->reactors+(index/mgr->one_reactor_sessionnum);
    reactor_pushclose(reactor, index%mgr->one_reactor_sessionnum);
}

struct nrmgr_send_msg_data*
ox_nrmgr_make_type_sendmsg(struct nr_mgr* mgr, const char* src, int len, char pool_index)
{
    struct nrmgr_send_msg_data* ret =  (struct nrmgr_send_msg_data*)ox_multi_pool_claim(mgr->sendmsg_pools, pool_index);
    
    if(ret != NULL)
    {
        ret->ref = 0;
        ret->data_len = 0;

        if(src != NULL && len > 0)
        {
            memcpy(ret->data, src, len);
            ret->data_len = len;

            assert((ret->data_len+sizeof(struct nrmgr_send_msg_data)) <= ox_multi_pool_config_len(mgr->sendmsg_pools, (char*)ret));
        }
    }

    return ret;
}

struct nrmgr_send_msg_data*
ox_nrmgr_make_sendmsg(struct nr_mgr* mgr, const char* src, int len)
{
    struct nrmgr_send_msg_data* ret = (struct nrmgr_send_msg_data*)ox_multi_pool_lenclaim(mgr->sendmsg_pools, len+sizeof(struct nrmgr_send_msg_data));
    
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
    struct multi_pool_s*    sendmsg_pools = reactor->mgr->sendmsg_pools;
    struct rwlist_s*    free_sendmsg_list = reactor->free_sendmsg_list;

    while((msg = (struct nrmgr_send_msg_data**)ox_rwlist_pop(free_sendmsg_list, 0)) != NULL)
    {
        struct nrmgr_send_msg_data* data = *msg;
        data->ref -= 1;
        if(data->ref == 0)
        {
            ox_multi_pool_reclaim(sendmsg_pools, (char*)data);
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
        ox_rwlist_flush(mgr->reactors[i].send_list);
        reactor_freesendmsg_handle(mgr->reactors+i);
    }
}

void
ox_nrmgr_sendmsg(struct nr_mgr* mgr, struct nrmgr_send_msg_data* data, int index)
{
    int inline_index = index % mgr->one_reactor_sessionnum;
    struct send_msg temp = {data, inline_index};
    struct net_reactor* reactor = mgr->reactors+(index/mgr->one_reactor_sessionnum);
    data->ref += 1;
    assert((data->data_len+sizeof(struct nrmgr_send_msg_data)) <= ox_multi_pool_config_len(mgr->sendmsg_pools, (char*)data));
    ox_rwlist_push(reactor->send_list, &temp);
}

static struct nrmgr_net_msg*
make_logicmsg(struct net_reactor* nr, enum nrmgr_net_msg_type type, int index, const char* data, int data_len)
{
    struct nrmgr_net_msg* msg = (struct nrmgr_net_msg*)ox_multi_pool_lenclaim(nr->logic_msgpools, data_len+sizeof(struct nrmgr_net_msg));

    if(msg != NULL)
    {
        msg->type = type;
        msg->index = index;
        msg->data_len = 0;

        if(data != NULL)
        {
            memcpy(msg->data, data, data_len);
            msg->data_len = data_len;
        }
    }

    return msg;
}

static void
reactor_pushenter(struct net_reactor* reactor, sock fd)
{
    ox_rwlist_push(reactor->enter_list, &fd);
    ox_rwlist_force_flush(reactor->enter_list);
}

static void
reactor_proc_enterlist(struct net_reactor* reactor)
{
    sock* fd_data = NULL;
    struct rwlist_s*    enter_list = reactor->enter_list;
    struct server_s* server = reactor->server;

    while((fd_data = (sock*)ox_rwlist_pop(enter_list, 0)) != NULL)
    {
        if(!server_register(server, *fd_data))
        {
            //ox_socket_close(*fd_data);
        }
    }
}

static void
session_destroy(struct net_reactor* reactor, int index)
{
    struct net_session_s* session = reactor->sessions+index;
    struct double_link_node_s* current = double_link_begin(&session->packet_list);
    struct double_link_node_s* end = double_link_end(&session->packet_list);

    while(current != end)
    {
        struct pending_send_msg_s* node = (struct pending_send_msg_s*)current;
        struct double_link_node_s* next = current->next;
        double_link_erase(&session->packet_list, current);
        ox_rwlist_push(reactor->free_sendmsg_list, &(node->msg));
        ox_type_pool_reclaim(reactor->pending_packet_pool, (char*)node);
        current = next;
    }

    double_link_init(&session->packet_list);

    server_close(reactor->server, index);
    session->active = false;
    reactor->active_num--;
    printf("reactor[%d], close index is %d , active num:%d\n", reactor->index, index, reactor->active_num);
}

static void
reactor_proc_closelist(struct net_reactor* reactor)
{
    sock* index_data = NULL;
    struct rwlist_s*    wait_close_list = reactor->wait_close_list;
    struct rwlist_s*    free_sendmsg_list = reactor->free_sendmsg_list;

    while((index_data = (sock*)ox_rwlist_pop(wait_close_list, 0)) != NULL)
    {
        int index = *index_data;
        session_destroy(reactor, index);
    }
}

static void
session_packet_flush(struct net_reactor* reactor, int index)
{
    struct net_session_s* session = reactor->sessions+index;
    if(session->active && session->bufnum > 0)
    {
        int bytes = server_sendv(reactor->server, index, session->bufs, session->lens, session->bufnum);
        if(bytes > 0)
        {
#ifdef PLATFORM_WINDOWS
            assert(false);
#endif
            session_sendfinish_callback(reactor->server, index, bytes);
        }
        else if(bytes == -1)
        {
            /*  直接处理socket断开    */
            printf("%d close index:%d\n", reactor->index, index);
            my_logic_on_close_pt(reactor->server, index);
        }
    }
}

static void
flushpacket_timerover(void* arg)
{
    struct net_session_s* session = (struct net_session_s*)arg;
    session->wait_flush = false;
    session_packet_flush(session->reactor, session->index);
}

static void 
insert_flush(struct net_reactor* reactor, int index)
{
    struct net_session_s* session = reactor->sessions+index;
    if(!session->wait_flush)
    {
        ox_timer_mgr_add(reactor->flush_timer, flushpacket_timerover, FLUSHTIMER_DELAY, session);
        session->wait_flush = true;
    }
}

static void
reactor_proc_sendlist(struct net_reactor* reactor)
{
    struct send_msg* msg = NULL;
    struct rwlist_s*    send_list = reactor->send_list;

    while((msg = (struct send_msg*)ox_rwlist_pop(send_list, 0)) != NULL)
    {
        int msg_index = msg->index;
        struct nrmgr_send_msg_data* data = msg->data;
        struct net_session_s* session = reactor->sessions+msg_index;

        if(session->active)
        {
            struct pending_send_msg_s* pending = (struct pending_send_msg_s*)ox_type_pool_claim(reactor->pending_packet_pool);

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

                    insert_flush(reactor, msg_index);
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
}

static void
reactor_proc_freelogiclist(struct net_reactor* reactor)
{
    struct nrmgr_net_msg** msg_p = NULL;
    struct nrmgr_net_msg* msg = NULL;
    struct rwlist_s*    free_logicmsg_list = reactor->free_logicmsg_list;
    struct multi_pool_s*    logic_msgpools = reactor->logic_msgpools;

    while((msg_p = (struct nrmgr_net_msg**)ox_rwlist_pop(free_logicmsg_list, 0)) != NULL)
    {
        msg = *msg_p;
        ox_multi_pool_reclaim(logic_msgpools, (char*)msg);
    }
}

static void
reactor_thread(void* arg)
{
    struct net_reactor* reactor = (struct net_reactor*)arg;

    while(reactor->thread == NULL || ox_thread_isrun(reactor->thread))
    {
        reactor_proc_enterlist(reactor);
        reactor_proc_closelist(reactor);
        reactor_proc_sendlist(reactor);
        reactor_proc_freelogiclist(reactor);

        ox_rwlist_flush(reactor->logic_msglist);
        /*  由用户给定网络库每次循环的超时时间   */
        server_pool(reactor->server, SOCKET_POLL_TIME);

        ox_timer_mgr_schedule(reactor->flush_timer);
    }
}

static void
my_logic_on_enter_pt(struct server_s* self, int index)
{
    struct net_reactor* reactor = (struct net_reactor*)server_getext(self);
    struct nrmgr_net_msg* msg = make_logicmsg(reactor, nrmgr_net_msg_connect, reactor->session_startindex+index, NULL, 0);
    assert(msg != NULL);

    if(msg != NULL)
    {
        reactor->sessions[index].active = true;
        reactor->active_num ++;
        printf("client enter, self[%p], reactor index:%d, inline index:%d, active num:%d\n", self, reactor->index, index, reactor->active_num);
        ox_rwlist_push(reactor->logic_msglist, &msg);
    }
}

static void
my_logic_on_close_pt(struct server_s* self, int index)
{
    struct net_reactor* reactor = (struct net_reactor*)server_getext(self);
    if(reactor->sessions[index].active)
    {
        struct nrmgr_net_msg* msg = make_logicmsg(reactor, nrmgr_net_msg_close, reactor->session_startindex+index, NULL, 0);
        assert(msg != NULL);

        if(msg != NULL)
        {
            reactor->sessions[index].active = false;
            printf("client socket closed, self[%p], reactor index:%d, inline index:%d\n", self, reactor->index, index);
            ox_rwlist_push(reactor->logic_msglist, &msg);
        }
    }
}

static int
my_logic_on_recved_pt(struct server_s* self, int index, const char* buffer, int len)
{
    struct net_reactor* reactor = (struct net_reactor*)server_getext(self);
    int proc_len = 0;
    int left_len = len;
    const char* check_buffer = buffer;
    struct rwlist_s*    logic_msglist = reactor->logic_msglist;

    while(left_len > 0)
    {
        int check_len = (reactor->check_packet)(check_buffer, left_len);
        if(check_len > 0)
        {
            struct nrmgr_net_msg* msg = make_logicmsg(reactor, nrmgr_net_msg_data, reactor->session_startindex+index, check_buffer, check_len);
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
session_sendfinish_callback(struct server_s* self, int index, int bytes)
{
    struct net_reactor* reactor = (struct net_reactor*)server_getext(self);
    struct net_session_s* session = reactor->sessions+index;

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
                    ox_type_pool_reclaim(reactor->pending_packet_pool, (char*)node);
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
                session_packet_flush(reactor, index);
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