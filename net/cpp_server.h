#ifndef _CPP_SERVER_H
#define _CPP_SERVER_H

#include <stdint.h>
#include <functional>

#include "thread_reactor.h"

class CppServer
{
    typedef std::function<int(CppServer&, void* ud, const char* buffer, int len)> PACKET_CHECK;
    typedef std::function<void(CppServer&, struct nrmgr_net_msg* msg)>  PACKET_HANDLE;

public:
    CppServer();

    void            create(int port, int thread_num, int rbsize, PACKET_CHECK handle);

    void            setMsgHandle(PACKET_HANDLE callback);

    void            logicPoll(int64_t ms);

    struct nrmgr_send_msg_data* makeSendMsg(const char* src, int len);
    void            sendMsg(struct net_session_s* session, struct nrmgr_send_msg_data* msg);

private:
    void            listen();

    static  int     s_check(struct nr_mgr* mgr, void* ud, const char* buffer, int len);
    static  void    s_handle(struct nr_mgr* mgr, struct nrmgr_net_msg*);
    static  void    s_listen(void* arg);

private:
    int             m_port;
    struct nr_mgr*  m_server;

    PACKET_CHECK    m_check;
    PACKET_HANDLE   m_handle;
};

#endif