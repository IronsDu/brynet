#ifndef _SOCKETLIBTYPES_H_INCLUDED_
#define _SOCKETLIBTYPES_H_INCLUDED_

#include "Platform.h"

#if defined PLATFORM_WINDOWS
#include <WinError.h>
#include <winsock2.h>
#include <winsock.h>
#include <Ws2tcpip.h>
#include <errno.h>

#elif defined PLATFORM_LINUX
#include <signal.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/eventfd.h>
#include <sys/uio.h>
#endif

#if defined PLATFORM_WINDOWS

typedef SOCKET sock;
#define sErrno WSAGetLastError()
#define S_ENOTSOCK WSAENOTSOCK
#define S_EWOULDBLOCK WSAEWOULDBLOCK
#define S_EINTR WSAEINTR
#define S_ECONNABORTED WSAECONNABORTED

#elif defined PLATFORM_LINUX

#define sErrno errno
#define S_ENOTSOCK EBADF
#define S_EWOULDBLOCK EAGAIN
#define S_EINTR EINTR
#define S_ECONNABORTED ECONNABORTED
typedef int sock;
#define SOCKET_ERROR (-1)

#endif

typedef unsigned short int port;
typedef unsigned long int ipaddress;
#define IP_SIZE (20)

#endif
