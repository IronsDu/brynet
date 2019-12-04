#pragma once

#include <brynet/base/Platform.hpp>

#ifdef BRYNET_PLATFORM_WINDOWS
#include <winsock2.h>
#include <WinError.h>
#include <winsock.h>
#include <Ws2tcpip.h>
#include <errno.h>

#elif defined BRYNET_PLATFORM_LINUX
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

#elif defined BRYNET_PLATFORM_DARWIN
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

#ifdef BRYNET_PLATFORM_WINDOWS
typedef SOCKET BrynetSocketFD;
#define BRYNET_ERRNO WSAGetLastError()
#define BRYNET_ENOTSOCK WSAENOTSOCK
#define BRYNET_EWOULDBLOCK WSAEWOULDBLOCK
#define BRYNET_EINTR WSAEINTR
#define BRYNET_ECONNABORTED WSAECONNABORTED
#define BRYNET_SOCKET_ERROR SOCKET_ERROR
#define BRYNET_INVALID_SOCKET INVALID_SOCKET

#elif defined BRYNET_PLATFORM_LINUX || defined BRYNET_PLATFORM_DARWIN
#define BRYNET_ERRNO errno
#define BRYNET_ENOTSOCK EBADF
#define BRYNET_EWOULDBLOCK EAGAIN
#define BRYNET_EINTR EINTR
#define BRYNET_ECONNABORTED ECONNABORTED
typedef int BrynetSocketFD;
#define BRYNET_SOCKET_ERROR (-1)
#define BRYNET_INVALID_SOCKET (-1)
#endif
