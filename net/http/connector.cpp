#include <assert.h>
#include <set>
#include <vector>
#include <map>

using namespace std;

#include "socketlibfunction.h"
#include "fdset.h"
#include "thread.h"
#include "rwlist.h"
#include "systemlib.h"

#include "connector.h"

class ConnectAddr
{
public:
    ConnectAddr(const char* ip, int port) : m_ip(ip), m_port(port)
    {

    }

    const string&   getIP() const
    {
        return m_ip;
    }
    
    int             getPort() const
    {
        return m_port;
    }

private:
    string  m_ip;
    int     m_port;
};

class ThreadConnector
{
public:
    ThreadConnector()
    {
        m_reqs = ox_rwlist_new(1024, sizeof(ConnectAddr*), 0);
        m_sockets = ox_rwlist_new(1024, sizeof(sock), 0);
        m_thread = NULL;
    }

    ~ThreadConnector()
    {
        stop();
    }

    void                startThread()
    {
        m_thread = ox_thread_new(ThreadConnector::s_thread, this);
    }

    void                stop()
    {
        if(m_thread != NULL)
        {
            ox_thread_delete(m_thread);
            m_thread = NULL;
        }
    }

    static sock socket_non_connect(const char* server_ip, int port, int overtime)
    {
        struct sockaddr_in server_addr;
        sock clientfd = SOCKET_ERROR;
        bool connect_ret = true;

        ox_socket_init();

        clientfd = socket(AF_INET, SOCK_STREAM, 0);
        ox_socket_nonblock(clientfd);

        if(clientfd != SOCKET_ERROR)
        {
            server_addr.sin_family = AF_INET;
            server_addr.sin_addr.s_addr = inet_addr(server_ip);
            server_addr.sin_port = htons(port);

            if(connect(clientfd, (struct sockaddr*)&server_addr, sizeof(struct sockaddr)) < 0)
            {
                int check_error = 0;

#if defined PLATFORM_WINDOWS
                check_error = WSAEWOULDBLOCK;
#else
                check_error = EINPROGRESS;
#endif

                if(check_error != sErrno)
                {
                    connect_ret = false;
                }
                else
                {
                    struct fdset_s* fdset = ox_fdset_new();
                    bool canwrite = false;
                    bool canread = false;

                    connect_ret = false;
                    ox_fdset_add(fdset, clientfd, ReadCheck | WriteCheck);
                    ox_fdset_poll(fdset, overtime);

                    canwrite = ox_fdset_check(fdset, clientfd, WriteCheck);
                    canread = ox_fdset_check(fdset, clientfd, ReadCheck);

                    if(canwrite)
                    {
                        if(canread)
                        {
                            int error;
                            int len = sizeof(error);
                            if(getsockopt(clientfd, SOL_SOCKET, SO_ERROR, (char*)&error, (socklen_t*)&len) >= 0)
                            {
                                connect_ret = true;
                            }
                        }
                        else
                        {
                            connect_ret = true;
                        }
                    }

                    ox_fdset_delete(fdset);
                }
            }
        }

        if(!connect_ret)
        {
            ox_socket_close(clientfd);
            clientfd = SOCKET_ERROR;
        }

        return clientfd;
    }

    bool                isConnectSuccess(struct fdset_s* fdset, sock clientfd)
    {
        bool canread = ox_fdset_check(fdset, clientfd, ReadCheck);
        bool canwrite = ox_fdset_check(fdset, clientfd, WriteCheck);

        bool connect_ret = false;

        if(canwrite)
        {
            if(canread)
            {
                int error;
                int len = sizeof(error);
                if(getsockopt(clientfd, SOL_SOCKET, SO_ERROR, (char*)&error, (socklen_t*)&len) >= 0)
                {
                    connect_ret = true;
                }
            }
            else
            {
                connect_ret = true;
            }
        }

        return connect_ret;
    }

    void                pollConnectRequest(struct fdset_s* fdset, int timeout)
    {
        ox_fdset_poll(fdset, timeout);

        fd_set* write_result = ox_fdset_getresult(fdset, WriteCheck);

        vector<sock>    complete_fds;   /*  完成队列    */

#if defined PLATFORM_WINDOWS
        {
            int fdcount = (int)write_result->fd_count;
            for(int i = 0; i < fdcount; ++i)
            {
                sock clientfd = write_result->fd_array[i];

                if(isConnectSuccess(fdset, clientfd))
                {
                    complete_fds.push_back(clientfd);
                }
            }
        }
#else
        {
            for(std::set<sock>::iterator it = m_connecting_fds.begin(); it != m_connecting_fds.end(); ++it)
            {
                sock clientfd = *it;
                if(isConnectSuccess(fdset, clientfd))
                {
                    complete_fds.push_back(clientfd);
                }
            }
        }
#endif

        for(size_t i = 0; i < complete_fds.size(); ++i)
        {
            sock fd = complete_fds[i];
            ox_rwlist_push(m_sockets, &fd);
            ox_rwlist_force_flush(m_sockets);

            ox_fdset_del(fdset, fd, ReadCheck | WriteCheck);

            std::set<sock>::iterator cfit = m_connecting_fds.find(fd);
            if(cfit != m_connecting_fds.end())
            {
                m_connecting_fds.erase(cfit);
            }

            map<sock, int64_t>::iterator ctit = m_connecting_time.find(fd);
            if(ctit != m_connecting_time.end())
            {
                m_connecting_time.erase(ctit);
            }
        }
    }

    void                run()
    {
        struct fdset_s* fdset = ox_fdset_new();
        int64_t save_time = ox_getnowtime();

        while(m_thread == NULL || ox_thread_isrun(m_thread))
        {
            if(false)
            {
                ConnectAddr** pp = (ConnectAddr**)ox_rwlist_pop(m_reqs, 1);
                if(pp != NULL)
                {
                    ConnectAddr* addr = *pp;
                    sock fd = socket_non_connect(addr->getIP().c_str(), addr->getPort(), 5000);
                    ox_rwlist_push(m_sockets, &fd);
                    ox_rwlist_force_flush(m_sockets);

                    if(fd == SOCKET_ERROR)
                    {
                        ;
                    }

                    delete addr;
                }
                else
                {
                    ox_rwlist_flush(m_sockets);
                }
            }
            else
            {
                pollConnectRequest(fdset, 1);

                while(m_connecting_fds.size() < FD_SETSIZE)
                {
                    ConnectAddr** pp = (ConnectAddr**)ox_rwlist_pop(m_reqs, 1);
                    if(pp != NULL)
                    {
                        bool add_success = false;
                        ConnectAddr* addr = *pp;

                        struct sockaddr_in server_addr;
                        sock clientfd = SOCKET_ERROR;

                        ox_socket_init();

                        clientfd = socket(AF_INET, SOCK_STREAM, 0);
                        ox_socket_nonblock(clientfd);

                        if(clientfd != SOCKET_ERROR)
                        {
                            server_addr.sin_family = AF_INET;
                            server_addr.sin_addr.s_addr = inet_addr(addr->getIP().c_str());
                            server_addr.sin_port = htons(addr->getPort());

                            if(connect(clientfd, (struct sockaddr*)&server_addr, sizeof(struct sockaddr)) < 0)
                            {
                                int check_error = 0;
#if defined PLATFORM_WINDOWS
                                check_error = WSAEWOULDBLOCK;
#else
                                check_error = EINPROGRESS;
#endif

                                if(check_error != sErrno)
                                {
                                    ox_socket_close(clientfd);
                                }
                                else
                                {
                                    m_connecting_time[clientfd] = ox_getnowtime();
                                    m_connecting_fds.insert(clientfd);
                                    ox_fdset_add(fdset, clientfd, ReadCheck | WriteCheck);

                                    add_success = true;
                                }
                            }
                        }

                        delete addr;

                        if(!add_success)
                        {
                            /*  如果没有链接成功，则返回一个失败通知  */
                            sock failed_fd = SOCKET_ERROR;
                            ox_rwlist_push(m_sockets, &failed_fd);
                            ox_rwlist_force_flush(m_sockets);
                        }
                    }
                    else
                    {
                        break;
                    }
                }

                int64_t now_time = ox_getnowtime();
                /*  处理超时的链接请求   */
                for(map<sock, int64_t>::iterator it = m_connecting_time.begin(); it != m_connecting_time.end(); )
                {
                    if((now_time - it->second) >= 5000)
                    {
                        sock fd = it->first;

                        ox_fdset_del(fdset, fd, ReadCheck | WriteCheck);

                        std::set<sock>::iterator cfit = m_connecting_fds.find(fd);
                        if(cfit != m_connecting_fds.end())
                        {
                            m_connecting_fds.erase(cfit);
                        }

                        m_connecting_time.erase(it++);

                        ox_socket_close(fd);

                        {
                            /*  如果超时，则返回一个失败通知  */
                            sock failed_fd = SOCKET_ERROR;
                            ox_rwlist_push(m_sockets, &failed_fd);
                            ox_rwlist_force_flush(m_sockets);
                        }
                    }
                    else
                    {
                        ++it;
                    }
                }

                ox_rwlist_flush(m_sockets);
            }
        }

        return;
    }

    /*  放入一个连接请求    */
    void                pushReq(const char* ip, int port)
    {
        ConnectAddr* addr = new ConnectAddr(ip, port);
        ox_rwlist_push(m_reqs, &addr);
        ox_rwlist_force_flush(m_reqs);
    }

    /*  flush请求队列   */
    void                flushList()
    {
        ox_rwlist_flush(m_reqs);
    }

    /*  获取一个已连接的描述符 */
    bool                popSocket(sock& outsock)
    {
        bool ret = false;
        sock* psock = (sock*)ox_rwlist_pop(m_sockets, 0);
        if(psock != NULL)
        {
            outsock = *psock;
            ret = true;
        }

        return ret;
    }

    static  void    s_thread(void* arg)
    {
        ThreadConnector*    tc = (ThreadConnector*)arg;
        tc->run();
    }

private:
    struct rwlist_s*    m_reqs;     /*  请求列表    */
    struct rwlist_s*    m_sockets;  /*  完成列表    */

    struct thread_s*    m_thread;

    map<sock, int64_t>  m_connecting_time;
    std::set<sock>      m_connecting_fds;
};

Connector::Connector()
{
    m_threadConnector = new ThreadConnector;
    m_threadConnector->startThread();
}

Connector::~Connector()
{
    if(m_threadConnector != NULL)
    {
        delete m_threadConnector;
        m_threadConnector = NULL;
    }

    while(!m_reqs.empty())
    {
        non_connect_req temp = *(m_reqs.begin());
        delete temp.ud;
        m_reqs.pop_front();
    }
}

void Connector::connect(const char* ip, int port, int ms, ConnectorBaseReq* ud)
{
    m_threadConnector->pushReq(ip, port);

    non_connect_req temp = {ud};
    m_reqs.push_back(temp);
}

void Connector::poll()
{
    while(true)
    {
        sock fd = SOCKET_ERROR;
        bool pop_ret = m_threadConnector->popSocket(fd);
        if(pop_ret)
        {
            if(m_reqs.empty())
            {
                /*  如果有完成反馈，那么m_reqs应该不会为empty  */
                assert(false);
                if(fd != SOCKET_ERROR)
                {
                    ox_socket_close(fd);
                }
            }
            else
            {
                non_connect_req temp = *(m_reqs.begin());
                m_reqs.pop_front();
                if(fd != SOCKET_ERROR)
                {
                    onConnectSuccess(fd, temp.ud);
                }
                else
                {
                    onConnectFailed(temp.ud);
                }
            }
        }
        else
        {
            m_threadConnector->flushList();
            break;
        }
    }
}
