#pragma once

#include <string.h>
#include <string>
#include <brynet/net/SocketLibTypes.hpp>

namespace brynet { namespace net { namespace base {

    static bool InitSocket()
    {
        bool ret = true;
#ifdef BRYNET_PLATFORM_WINDOWS
        static WSADATA g_WSAData;
        static bool WinSockIsInit = false;
        if (WinSockIsInit)
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
#elif defined BRYNET_PLATFORM_LINUX || defined BRYNET_PLATFORM_DARWIN
        signal(SIGPIPE, SIG_IGN);
#endif

        return ret;
    }

    static void DestroySocket()
    {
#ifdef BRYNET_PLATFORM_WINDOWS
        WSACleanup();
#endif
    }

    static int  SocketNodelay(BrynetSocketFD fd)
    {
        const int flag = 1;
        return ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (const char*)&flag, sizeof(flag));
    }

    static bool SocketBlock(BrynetSocketFD fd)
    {
        int err;
        unsigned long ul = false;
#ifdef BRYNET_PLATFORM_WINDOWS
        err = ioctlsocket(fd, FIONBIO, &ul);
#elif defined BRYNET_PLATFORM_LINUX || defined BRYNET_PLATFORM_DARWIN
        err = ioctl(fd, FIONBIO, &ul);
#endif

        return err != BRYNET_SOCKET_ERROR;
    }

    static bool SocketNonblock(BrynetSocketFD fd)
    {
        int err;
        unsigned long ul = true;
#ifdef BRYNET_PLATFORM_WINDOWS
        err = ioctlsocket(fd, FIONBIO, &ul);
#elif defined BRYNET_PLATFORM_LINUX || defined BRYNET_PLATFORM_DARWIN
        err = ioctl(fd, FIONBIO, &ul);
#endif

        return err != BRYNET_SOCKET_ERROR;
    }

    static int  SocketSetSendSize(BrynetSocketFD fd, int sd_size)
    {
        return ::setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (const char*)&sd_size, sizeof(sd_size));
    }

    static int  SocketSetRecvSize(BrynetSocketFD fd, int rd_size)
    {
        return ::setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (const char*)&rd_size, sizeof(rd_size));
    }

    static BrynetSocketFD SocketCreate(int af, int type, int protocol)
    {
        return ::socket(af, type, protocol);
    }

    static void SocketClose(BrynetSocketFD fd)
    {
#ifdef BRYNET_PLATFORM_WINDOWS
        ::closesocket(fd);
#elif defined BRYNET_PLATFORM_LINUX || defined BRYNET_PLATFORM_DARWIN
        ::close(fd);
#endif
    }

    static BrynetSocketFD Connect(bool isIPV6, const std::string& server_ip, int port)
    {
        InitSocket();

        struct sockaddr_in ip4Addr = sockaddr_in();
        struct sockaddr_in6 ip6Addr = sockaddr_in6();
        struct sockaddr_in* paddr = &ip4Addr;
        int addrLen = sizeof(ip4Addr);

        BrynetSocketFD clientfd = isIPV6 ?
            SocketCreate(AF_INET6, SOCK_STREAM, 0) :
            SocketCreate(AF_INET, SOCK_STREAM, 0);

        if (clientfd == BRYNET_INVALID_SOCKET)
        {
            return clientfd;
        }

        bool ptonResult = false;
        if (isIPV6)
        {
            ip6Addr.sin6_family = AF_INET6;
            ip6Addr.sin6_port = htons(port);
            ptonResult = inet_pton(AF_INET6, 
                server_ip.c_str(), 
                &ip6Addr.sin6_addr) > 0;
            paddr = (struct sockaddr_in*) & ip6Addr;
            addrLen = sizeof(ip6Addr);
        }
        else
        {
            ip4Addr.sin_family = AF_INET;
            ip4Addr.sin_port = htons(port);
            ptonResult = inet_pton(AF_INET, 
                server_ip.c_str(), 
                &ip4Addr.sin_addr) > 0;
        }

        if (!ptonResult)
        {
            SocketClose(clientfd);
            return BRYNET_INVALID_SOCKET;
        }

        while (::connect(clientfd, (struct sockaddr*)paddr, addrLen) < 0)
        {
            if (EINTR == BRYNET_ERRNO)
            {
                continue;
            }

            SocketClose(clientfd);
            return BRYNET_INVALID_SOCKET;
        }

        return clientfd;
    }

    static BrynetSocketFD Listen(bool isIPV6, const char* ip, int port, int back_num)
    {
        InitSocket();

        struct sockaddr_in ip4Addr = sockaddr_in();
        struct sockaddr_in6 ip6Addr = sockaddr_in6();
        struct sockaddr_in* paddr = &ip4Addr;
        int addrLen = sizeof(ip4Addr);

        const auto socketfd = isIPV6 ?
            socket(AF_INET6, SOCK_STREAM, 0) :
            socket(AF_INET, SOCK_STREAM, 0);
        if (socketfd == BRYNET_INVALID_SOCKET)
        {
            return BRYNET_INVALID_SOCKET;
        }

        bool ptonResult = false;
        if (isIPV6)
        {
            ip6Addr.sin6_family = AF_INET6;
            ip6Addr.sin6_port = htons(port);
            ptonResult = inet_pton(AF_INET6, ip, &ip6Addr.sin6_addr) > 0;
            paddr = (struct sockaddr_in*) & ip6Addr;
            addrLen = sizeof(ip6Addr);
        }
        else
        {
            ip4Addr.sin_family = AF_INET;
            ip4Addr.sin_port = htons(port);
            ip4Addr.sin_addr.s_addr = INADDR_ANY;
            ptonResult = inet_pton(AF_INET, ip, &ip4Addr.sin_addr) > 0;
        }

        const int reuseaddr_value = 1;
        if (!ptonResult ||
            ::setsockopt(socketfd,
                SOL_SOCKET,
                SO_REUSEADDR,
                (const char*)&reuseaddr_value,
                sizeof(int)) < 0)
        {
            SocketClose(socketfd);
            return BRYNET_INVALID_SOCKET;
        }

        const int bindRet = ::bind(socketfd, (struct sockaddr*)paddr, addrLen);
        if (bindRet == BRYNET_SOCKET_ERROR ||
            listen(socketfd, back_num) == BRYNET_SOCKET_ERROR)
        {
            SocketClose(socketfd);
            return BRYNET_INVALID_SOCKET;
        }

        return socketfd;
    }

    static std::string getIPString(const struct sockaddr* sa)
    {
#ifdef BRYNET_PLATFORM_WINDOWS
        using PAddrType = PVOID;
#elif defined BRYNET_PLATFORM_LINUX || defined BRYNET_PLATFORM_DARWIN
        using PAddrType = const void*;
#endif
        char tmp[INET6_ADDRSTRLEN] = { 0 };
        switch (sa->sa_family)
        {
        case AF_INET:
            inet_ntop(AF_INET, (PAddrType)(&(((const struct sockaddr_in*)sa)->sin_addr)),
                tmp, sizeof(tmp));
            break;
        case AF_INET6:
            inet_ntop(AF_INET6, (PAddrType)(&(((const struct sockaddr_in6*)sa)->sin6_addr)),
                tmp, sizeof(tmp));
            break;
        default:
            return "Unknown AF";
        }

        return tmp;
    }

    static std::string GetIPOfSocket(BrynetSocketFD fd)
    {
#ifdef BRYNET_PLATFORM_WINDOWS
        struct sockaddr name = sockaddr();
        int namelen = sizeof(name);
        if (::getpeername(fd, (struct sockaddr*) & name, &namelen) == 0)
        {
            return getIPString(&name);
        }
#elif defined BRYNET_PLATFORM_LINUX || defined BRYNET_PLATFORM_DARWIN
        struct sockaddr_in name = sockaddr_in();
        socklen_t namelen = sizeof(name);
        if (::getpeername(fd, (struct sockaddr*) & name, &namelen) == 0)
        {
            return getIPString((const struct sockaddr*) & name);
        }
#endif

        return "";
    }

    static int SocketSend(BrynetSocketFD fd, const char* buffer, int len)
    {
        int transnum = ::send(fd, buffer, len, 0);
        if (transnum < 0 && BRYNET_EWOULDBLOCK == BRYNET_ERRNO)
        {
            transnum = 0;
        }

        /*  send error if transnum < 0  */
        return transnum;
    }

    static BrynetSocketFD Accept(BrynetSocketFD listenSocket, struct sockaddr* addr, socklen_t* addrLen)
    {
        return ::accept(listenSocket, addr, addrLen);
    }

    static struct sockaddr_in6 getPeerAddr(BrynetSocketFD sockfd)
    {
        struct sockaddr_in6 peeraddr = sockaddr_in6();
        auto addrlen = static_cast<socklen_t>(sizeof peeraddr);
        if (::getpeername(sockfd, (struct sockaddr*)(&peeraddr), &addrlen) < 0)
        {
            return peeraddr;
        }
        return peeraddr;
    }

    static struct sockaddr_in6 getLocalAddr(BrynetSocketFD sockfd)
    {
        struct sockaddr_in6 localaddr = sockaddr_in6();
        auto addrlen = static_cast<socklen_t>(sizeof localaddr);
        if (::getsockname(sockfd, (struct sockaddr*)(&localaddr), &addrlen) < 0)
        {
            return localaddr;
        }
        return localaddr;
    }

    static bool IsSelfConnect(BrynetSocketFD fd)
    {
        struct sockaddr_in6 localaddr = getLocalAddr(fd);
        struct sockaddr_in6 peeraddr = getPeerAddr(fd);

        if (localaddr.sin6_family == AF_INET)
        {
            const struct sockaddr_in* laddr4 = reinterpret_cast<struct sockaddr_in*>(&localaddr);
            const struct sockaddr_in* raddr4 = reinterpret_cast<struct sockaddr_in*>(&peeraddr);
            return laddr4->sin_port == raddr4->sin_port
                && laddr4->sin_addr.s_addr == raddr4->sin_addr.s_addr;
        }
        else if (localaddr.sin6_family == AF_INET6)
        {
#ifdef BRYNET_PLATFORM_WINDOWS
            return localaddr.sin6_port == peeraddr.sin6_port
                && memcmp(&localaddr.sin6_addr.u.Byte, 
                    &peeraddr.sin6_addr.u.Byte, 
                    sizeof localaddr.sin6_addr.u.Byte) == 0;
#elif defined BRYNET_PLATFORM_LINUX || defined BRYNET_PLATFORM_DARWIN
            return localaddr.sin6_port == peeraddr.sin6_port
                && memcmp(&localaddr.sin6_addr.s6_addr, 
                    &peeraddr.sin6_addr.s6_addr, 
                    sizeof localaddr.sin6_addr.s6_addr) == 0;
#endif
        }
        else
        {
            return false;
        }
    }

} } }
