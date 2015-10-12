#include <stdio.h>
#include <string.h>

#include "SocketLibFunction.h"

#if defined PLATFORM_WINDOWS
static WSADATA g_WSAData;
#else
#include <linux/poll.h>
#endif

bool
ox_socket_init(void)
{
    bool ret = true;
    #if defined PLATFORM_WINDOWS
    static bool WinSockIsInit = false;
    if(!WinSockIsInit)
    {
        if(WSAStartup(MAKEWORD(2,2), &g_WSAData) == 0)
        {
            WinSockIsInit = true;
        }
        else
        {
            ret = false;
        }
    }
    #else
    signal(SIGPIPE, SIG_IGN);
    #endif

    return ret;
}

void
ox_socket_destroy(void)
{
    #if defined PLATFORM_WINDOWS
    WSACleanup();
    #endif
}

int
ox_socket_nodelay(sock fd)
{
    int flag = 1;
    return setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(flag));
}

bool
ox_socket_nonblock(sock fd)
{
    int err;
    unsigned long ul = true;
    #if defined PLATFORM_WINDOWS
    err = ioctlsocket(fd, FIONBIO, &ul);
    #else
    err = ioctl(fd, FIONBIO, &ul);
    #endif

    return err != SOCKET_ERROR;
}

int
ox_socket_setsdsize(sock fd, int sd_size)
{
    return setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (const char*)&sd_size, sizeof(sd_size));
}

int
ox_socket_setrdsize(sock fd, int rd_size)
{
    return setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (const char*)&rd_size, sizeof(rd_size));
}

sock
ox_socket_connect(const char* server_ip, int port)
{
    struct sockaddr_in server_addr;
    sock clientfd = SOCKET_ERROR;

    ox_socket_init();

    clientfd = socket(AF_INET, SOCK_STREAM, 0);

    if(clientfd != SOCKET_ERROR)
    {
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = inet_addr(server_ip);
        server_addr.sin_port = htons(port);

        while(connect(clientfd, (struct sockaddr*)&server_addr, sizeof(struct sockaddr)) < 0)
        {
            int e = sErrno;
            if(EINTR == sErrno)
            {
                continue;
            }
            else
            {
                ox_socket_close(clientfd);
                clientfd = SOCKET_ERROR;
                break;
            }
        }
    }

    return clientfd;
}

sock
ox_socket_nonblockconnect(const char* server_ip, int port, int timeout)
{
    ox_socket_init();

    struct sockaddr_in server_addr;
    sock  clientfd = socket(AF_INET, SOCK_STREAM, 0);

    if (clientfd != SOCKET_ERROR)
    {
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = inet_addr(server_ip);
        server_addr.sin_port = htons(port);
        ox_socket_nonblock(clientfd);
        connect(clientfd, (struct sockaddr*)&server_addr, sizeof(struct sockaddr));

#ifdef PLATFORM_WINDOWS
        WSAPOLLFD pfd;
        pfd.fd = clientfd;
        pfd.events = POLLOUT;
        if (WSAPoll(&pfd, 1, timeout) != 1 || pfd.revents != POLLOUT)
        {
            ox_socket_close(clientfd);
            clientfd = SOCKET_ERROR;
        }
#else
#endif
    }

    return clientfd;
}

sock
ox_socket_listen(int port, int back_num)
{
    sock socketfd = SOCKET_ERROR;
    struct  sockaddr_in server_addr;
    int reuseaddr_value = 1;

    ox_socket_init();

    socketfd = socket(AF_INET, SOCK_STREAM, 0);

    if(socketfd != SOCKET_ERROR)
    {
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        server_addr.sin_addr.s_addr = INADDR_ANY;

        if (setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuseaddr_value, sizeof(int)) >= 0)
        {
            int bindRet = bind(socketfd, (struct sockaddr*)&server_addr, sizeof(struct sockaddr));
            if (bindRet == SOCKET_ERROR ||
                listen(socketfd, back_num) == SOCKET_ERROR)
            {
                ox_socket_close(socketfd);
                socketfd = SOCKET_ERROR;
            }
        }
        else
        {
            ox_socket_close(socketfd);
            socketfd = SOCKET_ERROR;
        }
    }


    return socketfd;
}

void
ox_socket_close(sock fd)
{
    #if defined PLATFORM_WINDOWS
    closesocket(fd);
    #else
    close(fd);
    #endif
}

const char*
ox_socket_getipoffd(sock fd)
{
#if defined PLATFORM_WINDOWS
    struct sockaddr_in name;
    int namelen = sizeof(name);
    if(getpeername(fd, (struct sockaddr*)&name, &namelen) == 0)
    {
        return inet_ntoa(name.sin_addr);
    }
#else
    struct sockaddr_in name;
    socklen_t namelen = sizeof(name);
    if(getpeername(fd, (struct sockaddr*)&name, &namelen) == 0)
    {
        return inet_ntoa(name.sin_addr);
    }
#endif

    return "";
}

const char*
ox_socket_getipstr(unsigned int ip)
{
    struct in_addr addr;
    addr.s_addr = htonl(ip);
    return inet_ntoa(addr);
}

int
ox_socket_send(sock fd, const char* buffer, int len)
{
    int transnum = send(fd, buffer, len, 0);
    if(transnum < 0 && S_EWOULDBLOCK == sErrno)
    {
        transnum = 0;
    }

    /*  send error if transnum < 0  */
    return transnum;
}
