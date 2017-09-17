#include <stdio.h>
#include <string.h>

#include <brynet/net/SocketLibFunction.h>

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
    if(WinSockIsInit)
    {
        return true;
    }
    if (WSAStartup(MAKEWORD(2, 2), &g_WSAData) == 0)
    {
        WinSockIsInit = true;
    }
    else
    {
        ret = false;
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
ox_socket_block(sock fd)
{
    int err;
    unsigned long ul = false;
#if defined PLATFORM_WINDOWS
    err = ioctlsocket(fd, FIONBIO, &ul);
#else
    err = ioctl(fd, FIONBIO, &ul);
#endif

    return err != SOCKET_ERROR;
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
ox_socket_connect(bool isIPV6, const char* server_ip, int port)
{
    struct sockaddr_in ip4Addr;
    struct sockaddr_in6 ip6Addr;
    struct sockaddr_in* paddr = &ip4Addr;
    int addrLen = sizeof(ip4Addr);

    sock clientfd = SOCKET_ERROR;
    ox_socket_init();

    clientfd = isIPV6 ? 
        ox_socket_create(AF_INET6, SOCK_STREAM, 0) : 
        ox_socket_create(AF_INET, SOCK_STREAM, 0);

    if(clientfd == SOCKET_ERROR)
    {
        return clientfd;
    }

    bool ptonResult = false;
    if (isIPV6)
    {
        memset(&ip6Addr, 0, sizeof(ip6Addr));
        ip6Addr.sin6_family = AF_INET6;
        ip6Addr.sin6_port = htons(port);
        ptonResult = inet_pton(AF_INET6, server_ip, &ip6Addr.sin6_addr) > 0;
        paddr = (struct sockaddr_in*)&ip6Addr;
        addrLen = sizeof(ip6Addr);
    }
    else
    {
        ip4Addr.sin_family = AF_INET;
        ip4Addr.sin_port = htons(port);
        ptonResult = inet_pton(AF_INET, server_ip, &ip4Addr.sin_addr) > 0;
    }

    if (!ptonResult)
    {
        ox_socket_close(clientfd);
        clientfd = SOCKET_ERROR;
        return SOCKET_ERROR;
    }

    while (connect(clientfd, (struct sockaddr*)paddr, addrLen) < 0)
    {
        int e = sErrno;
        if (EINTR == sErrno)
        {
            continue;
        }

        ox_socket_close(clientfd);
        clientfd = SOCKET_ERROR;
        break;
    }

    return clientfd;
}

sock
ox_socket_listen(bool isIPV6, const char* ip, int port, int back_num)
{
    sock socketfd = SOCKET_ERROR;
    struct  sockaddr_in ip4Addr;
    struct sockaddr_in6 ip6Addr;
    struct sockaddr_in* paddr = &ip4Addr;
    int addrLen = sizeof(ip4Addr);

    ox_socket_init();

    socketfd = isIPV6 ? 
        socket(AF_INET6, SOCK_STREAM, 0) : 
        socket(AF_INET, SOCK_STREAM, 0);
    if(socketfd == SOCKET_ERROR)
    {
        return SOCKET_ERROR;
    }

    bool ptonResult = false;
    if (isIPV6)
    {
        memset(&ip6Addr, 0, sizeof(ip6Addr));
        ip6Addr.sin6_family = AF_INET6;
        ip6Addr.sin6_port = htons(port);
        ptonResult = inet_pton(AF_INET6, ip, &ip6Addr.sin6_addr) > 0;
        paddr = (struct sockaddr_in*)&ip6Addr;
        addrLen = sizeof(ip6Addr);
    }
    else
    {
        ip4Addr.sin_family = AF_INET;
        ip4Addr.sin_port = htons(port);
        ip4Addr.sin_addr.s_addr = INADDR_ANY;
        ptonResult = inet_pton(AF_INET, ip, &ip4Addr.sin_addr) > 0;
    }

    int reuseaddr_value = 1;
    if (!ptonResult || 
        setsockopt(socketfd, 
            SOL_SOCKET, 
            SO_REUSEADDR, 
            (const char *)&reuseaddr_value, 
            sizeof(int)) < 0)
    {
        ox_socket_close(socketfd);
        socketfd = SOCKET_ERROR;
        return SOCKET_ERROR;
    }

    int bindRet = bind(socketfd, (struct sockaddr*)paddr, addrLen);
    if (bindRet == SOCKET_ERROR ||
        listen(socketfd, back_num) == SOCKET_ERROR)
    {
        ox_socket_close(socketfd);
        socketfd = SOCKET_ERROR;
        return SOCKET_ERROR;
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

sock
ox_socket_accept(sock listenSocket, struct sockaddr* addr, socklen_t* addrLen)
{
#if defined PLATFORM_WINDOWS
    sock fd = accept(listenSocket, addr, addrLen);
    if(fd == INVALID_SOCKET)
    {
        fd = SOCKET_ERROR;
    }
    return fd;
#else
    return accept(listenSocket, addr, addrLen);
#endif
}

sock
ox_socket_create(int af, int type, int protocol)
{
#if defined PLATFORM_WINDOWS
    sock fd = socket(af, type, protocol);
    if(fd == INVALID_SOCKET)
    {
        fd = SOCKET_ERROR;
    }

    return fd;
#else
    return socket(af, type, protocol);
#endif
}
