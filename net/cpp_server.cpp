#include "socketlibfunction.h"
#include "socketlibtypes.h"
#include "thread.h"

#include "cpp_server.h"

CppServer::CppServer()
{
    m_server = nullptr;
}

void CppServer::addFD(int fd)
{
    ox_socket_nodelay(fd);
    ox_socket_nonblock(fd);
    ox_nrmgr_addfd(m_server, NULL, fd);
}

void CppServer::startListen(int port)
{
    m_port = port;
    ox_thread_new(s_listen, this);
}

void CppServer::create(int thread_num, int rbsize, PACKET_CHECK handle)
{
    m_check = handle;
    m_server = ox_create_nrmgr(thread_num, rbsize, s_check);
    ox_nrmgr_setuserdata(m_server, this);
}

void CppServer::setMsgHandle(PACKET_HANDLE callback)
{
    m_handle = callback;
}

struct nrmgr_send_msg_data* CppServer::makeSendMsg(const char* src, int len)
{
    return ox_nrmgr_make_sendmsg(m_server, src, len);
}

void CppServer::sendMsg(struct net_session_s* session, struct nrmgr_send_msg_data* msg)
{
    ox_nrmgr_sendmsg(m_server, msg, session);
}

void CppServer::logicPoll(int64_t ms)
{
    ox_nrmgr_logic_poll(m_server, s_handle, ms);
}

void CppServer::listen()
{
    sock client_fd = SOCKET_ERROR;
    struct sockaddr_in socketaddress;
    socklen_t size = sizeof(struct sockaddr);

    sock listen_fd = ox_socket_listen(m_port, 25);

    if (SOCKET_ERROR != listen_fd)
    {
        for (;;)
        {
            while ((client_fd = accept(listen_fd, (struct sockaddr*)&socketaddress, &size)) < 0)
            {
                if (EINTR == sErrno)
                {
                    continue;
                }
            }

            if (SOCKET_ERROR != client_fd)
            {
                addFD(client_fd);
            }
        }

        ox_socket_close(listen_fd);
        listen_fd = SOCKET_ERROR;
    }
    else
    {
        perror("listen failed\n");
    }
}

int CppServer::s_check(struct nr_mgr* mgr, void* ud, const char* buffer, int len)
{
    CppServer* server = static_cast<CppServer*>(ox_nrmgr_getuserdata(mgr));
    return (server->m_check)(*server, ud, buffer, len);
}

void CppServer::s_handle(struct nr_mgr* mgr, struct nrmgr_net_msg* msg)
{
    CppServer* server = static_cast<CppServer*>(ox_nrmgr_getuserdata(mgr));
    (server->m_handle)(*server, msg);
}

void CppServer::s_listen(void* arg)
{
    CppServer* server = static_cast<CppServer*>(arg);
    server->listen();
}