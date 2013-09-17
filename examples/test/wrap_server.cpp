#include <iostream>
using namespace std;

#include "thread_reactor.h"
#include "thread.h"
#include "socketlibfunction.h"
#include "socketlibtypes.h"

#include "wrap_server.h"

static WrapServer*	wserver = NULL;

WrapServer::WrapServer()
{
	m_mgr = NULL;
	m_listenThread = NULL;

	wserver = this;
}

void WrapServer::create(const char* ip, int port, int thread_num, int client_num)
{
	int nums[] = {1280, 1280, 1280};
	int lens[] = {128, 128000, 128000};

	nr_server_msgpool_config config = 
	{
		nums,
		lens,
		3,
		nums,
		lens,
		3
	};

	m_mgr = ox_create_nrmgr(client_num,
				thread_num,
				10024,
				10024,
				s_check,
				config);
	
	m_port = port;
	
	m_listenThread = ox_thread_new(WrapServer::s_listen, this);
}

void WrapServer::poll(int ms)
{
	ox_nrmgr_logic_poll(m_mgr, WrapServer::s_handleMsg, ms);
}

void WrapServer::sendTo(int index, const char* buffer, int len)
{
	struct nrmgr_send_msg_data* sd_msg = ox_nrmgr_make_sendmsg(m_mgr, buffer, len);
	ox_nrmgr_sendmsg(m_mgr, sd_msg, index);
}

void WrapServer::listenThread()
{
	sock client_fd = SOCKET_ERROR;
  	struct sockaddr_in socketaddress;
    	socklen_t size = sizeof(struct sockaddr);

    	sock listen_fd = ox_socket_listen(m_port, 25);

    	if(SOCKET_ERROR != listen_fd)
    	{
        	for(;1;)
        	{
            		while((client_fd = accept(listen_fd, (struct sockaddr*)&socketaddress, &size)) < 0)
            		{
               		 	if(EINTR == sErrno)
                		{
                    			continue;
                		}
            		}

            		if(SOCKET_ERROR != client_fd)
           		{
				ox_socket_nonblock(client_fd);
                		ox_socket_nodelay(client_fd);
                		ox_nrmgr_addfd(m_mgr, client_fd);
            		}
        	}

        	listen_fd = SOCKET_ERROR;
    	}
    	else
    	{
        	cout << "listen failed" << endl;
    	}
}

void WrapServer::s_listen(void* arg)
{
	WrapServer*	wrap = (WrapServer*)arg;
	wrap->listenThread();
}

int WrapServer::s_check(const char* buffer, int len)
{
	return wserver->check(buffer, len);
}

void WrapServer::s_handleMsg(nr_mgr* mgr, nrmgr_net_msg* msg)
{
	if(msg->type == nrmgr_net_msg_connect)
	{
		wserver->onConnected(msg->index);
	}
	else if(msg->type == nrmgr_net_msg_close)
	{
		ox_nrmgr_closesession(mgr, msg->index);
		wserver->onDisconnected(msg->index);
	}
	else if(msg->type == nrmgr_net_msg_data)
	{
		wserver->onRecvdata(msg->index, msg->data, msg->data_len);
	}
}
