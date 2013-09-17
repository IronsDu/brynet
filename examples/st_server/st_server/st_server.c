#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "platform.h"
#include "server.h"
#include "rwlist.h"
#include "typepool.h"
#include "thread.h"
#include "socketlibtypes.h"
#include "socketlibfunction.h"

#ifdef PLATFORM_LINUX
#include "epollserver.h"
#else
#include "iocp.h"
#endif

#include "stack.h"
#include "multipool.h"
#include "double_link.h"
#include "st_server.h"

#define PENDING_POOL_SIZE (10024)

struct packet_pending_s
{
    struct double_link_node_s   node;
    int packet_id;
    int left_len;
    struct st_server_packet_s* packet;
};

#define MAX_SENDBUF_NUM (2048)

/*  最大累计数量  */
#define MAX_PENDING_NUM (20)

struct st_session_s
{
    struct double_link_s    packet_list;
    const char* bufs[MAX_SENDBUF_NUM];
    int lens[MAX_SENDBUF_NUM];
    int bufnum;
    bool active;

    int current_id; /*  分配的id   */
    int end_id;     /*  预期下一个发送id   */

    int logic_send_num; /*  逻辑层共send个数  */
    int net_send_num;   /*  网络层共send个数  */
};

struct st_server_s
{
    struct server_s* server;
    int num;

    struct rwlist_s* enter_list;
    struct multi_pool_s*    packet_pools;

    struct st_session_s* sessions;

    struct type_pool_s* pending_packet_pool;

    pfn_check_packet packet_check;
    pfn_packet_handle packet_handle;
    pfn_session_onenter onenter;
    pfn_session_onclose onclose;
};

static void
_logic_on_enter_callback(struct server_s* self, int index);

static void
_logic_on_close_callback(struct server_s* self, int index);

static int
_logic_on_recved_callback(struct server_s* self, int index, const char* buffer, int len);

static void
_cansend_callback(struct server_s* self, int index);

static void
_sendfinish_callback(struct server_s* self, int index, int bytes);

static void
st_packet_decref(struct st_server_s* self, struct st_server_packet_s*);

static void
session_reclaim_pendinglist(struct st_server_s* self, struct st_session_s* session);

struct st_server_s*
ox_stserver_create(
    int num,
    int rdsize,
    int sdsize,
    pfn_check_packet packet_check,
    pfn_packet_handle packet_handle,
    pfn_session_onenter onenter,
    pfn_session_onclose onclose,
    const struct st_server_msgpool_config config)
{
    struct st_server_s* ret = (struct st_server_s*)malloc(sizeof(*ret));

    ret->num = num;

    ret->enter_list = ox_rwlist_new(ret->num, sizeof(sock), num*2);
#ifdef PLATFORM_LINUX
    ret->server = epollserver_create(num, rdsize, sdsize, ret);
#else
    ret->server = iocp_create(num, rdsize, sdsize, ret);
#endif
    ret->packet_pools = ox_multi_pool_new(config.msg_pool_num, config.msg_pool_len, config.msg_pool_typemax, sizeof(struct st_server_packet_s));

    ret->sessions = (struct st_session_s*)malloc(sizeof(struct st_session_s)*ret->num);

    {
        int i = 0;
        for(; i < num; ++i)
        {
            ret->sessions[i].active = false;
            ret->sessions[i].bufnum = 0;
            ret->sessions[i].current_id = 0;
            ret->sessions[i].end_id = 0;
            ret->sessions[i].logic_send_num = 0;
            ret->sessions[i].net_send_num = 0;
            double_link_init(&ret->sessions[i].packet_list);
        }
    }

    ret->pending_packet_pool = ox_type_pool_new(PENDING_POOL_SIZE, sizeof(struct packet_pending_s));

    ret->packet_check = packet_check;
    ret->packet_handle = packet_handle;
    ret->onenter = onenter;
    ret->onclose = onclose;

    server_start(ret->server, _logic_on_enter_callback, _logic_on_close_callback, _logic_on_recved_callback, _cansend_callback, _sendfinish_callback);

    return ret;
}

static void
st_proc_enterlist(struct st_server_s* self)
{
    struct rwlist_s* enter_list = self->enter_list;
    sock* fd_data = NULL;
    while((fd_data = (sock*)ox_rwlist_pop(enter_list, 0)) != NULL)
    {
        //printf("reactor register, fd is %d \n", *fd_data);
        if(!server_register(self->server, *fd_data))
        {
            ox_socket_close(*fd_data);
        }
    }
}

void
ox_stserver_poll(struct st_server_s* self, int timeout)
{
    st_proc_enterlist(self);
    server_pool(self->server, timeout);
}

void
ox_stserver_delete(struct st_server_s* self)
{
    server_stop(self->server);
    ox_rwlist_delete(self->enter_list);
    ox_multi_pool_delete(self->packet_pools);

    ox_type_pool_delete(self->pending_packet_pool);
    free(self->sessions);
    free(self);
    self = NULL;
}

static void
st_close_help(struct st_server_s* st, int index)
{
    server_close(st->server, index);
    session_reclaim_pendinglist(st, st->sessions+index);
    st->sessions[index].active = false;
    st->sessions[index].bufnum = 0;
    st->sessions[index].current_id = 0;
    st->sessions[index].end_id = 0;
    st->sessions[index].logic_send_num = 0;
    st->sessions[index].net_send_num = 0;
}

void
ox_stserver_close(struct st_server_s* st, int index)
{
    struct st_session_s* session = st->sessions + index;
    if(session->active)
    {
        st_close_help(st, index);
    }
}

struct st_server_packet_s*
ox_stserver_claim_packet(struct st_server_s* self, int pool_index)
{
    struct st_server_packet_s* ret = (struct st_server_packet_s*)ox_multi_pool_claim(self->packet_pools, pool_index);

    assert(ret != NULL);

    if(ret != NULL)
    {
        ret->data_len = 0;
        ret->ref = 0;
    }

    return ret;
}

struct st_server_packet_s*
ox_stserver_claim_lenpacket(struct st_server_s* self, int len)
{
    struct st_server_packet_s* ret = (struct st_server_packet_s*)ox_multi_pool_lenclaim(self->packet_pools, len+sizeof(struct st_server_packet_s));

    assert(ret != NULL);

    if(ret != NULL)
    {
        ret->ref = 0;
        ret->data_len = 0;
    }

    return ret;
}

static bool check_list(struct st_session_s* session)
{
    bool ret = true;

    int i = 0;
    for(; i < session->bufnum; ++i)
    {
        int j = i+1;
        for(;j < session->bufnum; ++j)
        {
            if(session->bufs[i] == session->bufs[j])
            {
                printf("fuck i: %d j : %d \n", i, j);
                assert(false);
                ret = false;
                break;
            }
        }

        if(!ret)
        {
            break;
        }
    }

    return ret;
}

static void check_packet_list(struct st_session_s* session)
{
    struct double_link_node_s* current = double_link_begin(&session->packet_list);
    struct double_link_node_s* end = double_link_end(&session->packet_list);

    while(false && current != end)
    {
        struct packet_pending_s* node = (struct packet_pending_s*)current;
        struct double_link_node_s* next = current->next;

        /*  链表头结点的id应该等于预计已发送的packet id */
        assert(node->packet_id == session->end_id);

        current = next;
        break;
    }
}

bool
ox_stserver_send(struct st_server_s* self, struct st_server_packet_s* packet, int index)
{
    bool ret = false;
    struct st_session_s* session = self->sessions+index;
    assert((packet->data_len+sizeof(struct st_server_packet_s)) <= ox_multi_pool_config_len(self->packet_pools, (char*)packet));
    assert(index >= 0 && index < self->num);
    assert(packet != NULL);

    if(session->active)
    {
        struct packet_pending_s* pending = (struct packet_pending_s*)ox_type_pool_claim(self->pending_packet_pool);
        /*  直接放入链表  */
        pending->node.next = pending->node.prior = NULL;
        ++packet->ref;
        pending->packet_id = session->current_id;
        session->current_id++;
        assert(session->current_id > 0);
        pending->packet = packet;
        pending->left_len = packet->data_len;
        double_link_push_back(&session->packet_list, (struct double_link_node_s*)pending);
        
        session->logic_send_num ++;

        check_packet_list(session);

        if(session->bufnum < MAX_SENDBUF_NUM)
        {
            session->bufs[session->bufnum] = packet->data;
            session->lens[session->bufnum] = pending->left_len;
            ++session->bufnum;
        }

        ret =  true;

        if(true || !check_list(session))
        {
            ret = false;
        }

        if(session->bufnum >= MAX_PENDING_NUM)
        {
            ox_stserver_flush(self, index);
        }

        check_packet_list(session);
    }
    else
    {
        assert(false);
    }

    return ret;
}

void
ox_stserver_flush(struct st_server_s* self, int index)
{
    struct st_session_s* session = self->sessions+index;

    if(session->active && session->bufnum > 0)
    {
        int bytes = server_sendv(self->server, index, session->bufs, session->lens, session->bufnum);
        if(bytes > 0)
        {
            #ifdef PLATFORM_WINDOWS
            assert(false);
            #endif
            _sendfinish_callback(self->server, index, bytes);
        }
        else if(bytes == -1)
        {
            /*  直接处理socket断开    */
            _logic_on_close_callback(self->server, index);
        }
    }
}

void
ox_stserver_check_reclaim(struct st_server_s* self, struct st_server_packet_s* packet)
{
    packet->ref -= 1;
    if(packet->ref < 0)
    {
        ox_multi_pool_reclaim(self->packet_pools, (char*)packet);
    }
}

void
ox_stserver_addfd(struct st_server_s* self, sock fd)
{
    ox_rwlist_push(self->enter_list, &fd);

    /*  无条件同步到共享队列  */
    ox_rwlist_force_flush(self->enter_list);
}

static void
_cansend_callback(struct server_s* self, int index)
{
    struct st_server_s* st = (struct st_server_s*)server_getext(self);
    //st_send_packet_help(st, st->sessions+index, index);
}

static void
_sendfinish_callback(struct server_s* self, int index, int bytes)
{
    struct st_server_s* st = (struct st_server_s*)server_getext(self);
    struct st_session_s* session = st->sessions+index;

    if(session->active)
    {
        {
            struct double_link_node_s* current = double_link_begin(&session->packet_list);
            struct double_link_node_s* end = double_link_end(&session->packet_list);

            check_packet_list(session);

            while(current != end && bytes > 0)
            {
                struct packet_pending_s* node = (struct packet_pending_s*)current;
                struct double_link_node_s* next = current->next;
                if(node->left_len <= bytes)
                {
                    /*  当前packet已经发送完毕  */
                    assert(node->packet_id == session->end_id);
                    session->end_id++;
                    double_link_erase(&session->packet_list, current);
                    st_packet_decref(st, node->packet);
                    bytes -= node->left_len;
                    ox_type_pool_reclaim(st->pending_packet_pool, (char*)node);
                    session->net_send_num++;
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

            check_packet_list(session);

            while(current != end && session->bufnum < MAX_SENDBUF_NUM)
            {
                struct packet_pending_s* node = (struct packet_pending_s*)current;
                struct double_link_node_s* next = current->next;
                session->bufs[session->bufnum] = node->packet->data+(node->packet->data_len-node->left_len);
                session->lens[session->bufnum] = node->left_len;
                session->bufnum++;

                current = next;
            }
            
            /*  没有完全发送完毕继续发送    */
            if(session->bufnum >= MAX_PENDING_NUM)
            {
                ox_stserver_flush(st, index);
            }

            check_packet_list(session);
        }
    }
}

static void
_logic_on_enter_callback(struct server_s* self, int index)
{
    struct st_server_s* st = (struct st_server_s*)server_getext(self);
    st->sessions[index].active = true;
    (st->onenter)(st, index);
}

static void
session_reclaim_pendinglist(struct st_server_s* self, struct st_session_s* session)
{
    struct double_link_node_s* begin = double_link_begin(&session->packet_list);
    struct double_link_node_s* end = double_link_end(&session->packet_list);

    while(begin != end)
    {
        struct packet_pending_s* node = (struct packet_pending_s*)begin;
        st_packet_decref(self, node->packet);
        ox_type_pool_reclaim(self->pending_packet_pool, (char*)node);
        begin = begin->next;
    }

    double_link_init(&session->packet_list);
}

static void
_logic_on_close_callback(struct server_s* self, int index)
{
    struct st_server_s* st = (struct st_server_s*)server_getext(self);
    st_close_help(st, index);
    (st->onclose)(st, index);
}

static int
_logic_on_recved_callback(struct server_s* self, int index, const char* buffer, int len)
{
    struct st_server_s* st = (struct st_server_s*)server_getext(self);

    int proc_len = 0;
    int left_len = len;
    const char* check_buffer = buffer;
    pfn_packet_handle callback = st->packet_handle;

    while(left_len > 0)
    {
        int check_len = (st->packet_check)(check_buffer, left_len);
        if(check_len > 0)
        {
            (callback)(st, index, check_buffer, check_len);

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
st_packet_decref(struct st_server_s* self, struct st_server_packet_s* packet)
{
    if((packet->ref -= 1) < 0)
    {
        ox_multi_pool_reclaim(self->packet_pools, (char*)packet);
    }
}
