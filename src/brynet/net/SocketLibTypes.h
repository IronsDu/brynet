#pragma once

#include <brynet/net/Platform.h>

#ifdef PLATFORM_WINDOWS
#include <winsock2.h>
#include <WinError.h>
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

#elif defined PLATFORM_DARWIN
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/event.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/uio.h>

#else
#error "Unsupported OS, please commit an issuse."
#endif

#ifdef PLATFORM_WINDOWS
typedef SOCKET sock;
#define sErrno WSAGetLastError()
#define S_ENOTSOCK WSAENOTSOCK
#define S_EWOULDBLOCK WSAEWOULDBLOCK
#define S_EINTR WSAEINTR
#define S_ECONNABORTED WSAECONNABORTED

#elif defined PLATFORM_LINUX || defined PLATFORM_DARWIN
#define sErrno errno
#define S_ENOTSOCK EBADF
#define S_EWOULDBLOCK EAGAIN
#define S_EINTR EINTR
#define S_ECONNABORTED ECONNABORTED
typedef int sock;
#define SOCKET_ERROR (-1)
#define INVALID_SOCKET (-1)
#endif

typedef unsigned short int port;
typedef unsigned long int ipaddress;
#define IP_SIZE (20)
