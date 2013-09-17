#ifndef _WRAP_SERVER_H
#define _WRAP_SERVER_H

struct nr_mgr;
struct thread_s;
struct nrmgr_net_msg;

class WrapServer
{
public:
	WrapServer();

	void	create(const char* ip, int port, int thread_num, int client_max);
	void	poll(int ms);
	void	sendTo(int index, const char* buffer, int len);
 
private:
	static	void	s_listen(void* arg);
	static	int	s_check(const char* buffer, int len);
	static	void	s_handleMsg(nr_mgr* mgr, nrmgr_net_msg*);

private:
	void		listenThread();
	virtual	int	check(const char* buffer, int len) = 0;

	virtual	void	onConnected(int) = 0;
	virtual	void	onDisconnected(int) = 0;
	virtual	void	onRecvdata(int, const char* data, int len) = 0;

private:
	int		m_port;
	nr_mgr*		m_mgr;
	thread_s*	m_listenThread;	
};

#endif
