#ifndef DODO_NET_SOCKETLIBFUNCTION_H_
#define DODO_NET_SOCKETLIBFUNCTION_H_

#include <stdbool.h>
#include "SocketLibTypes.h"

#ifdef  __cplusplus
extern "C" {
#endif

bool ox_socket_init();
void ox_socket_destroy();
int  ox_socket_nodelay(sock fd);
bool ox_socket_block(sock fd);
bool ox_socket_nonblock(sock fd);
int  ox_socket_setsdsize(sock fd, int sd_size);
int  ox_socket_setrdsize(sock fd, int rd_size);
sock ox_socket_connect(bool isIPV6, const char* server_ip, int port);
sock ox_socket_nonblockconnect(const char* server_ip, int port, int timeout);
sock ox_socket_listen(bool isIPV6, const char* ip, int port, int back_num);
void ox_socket_close(sock fd);
const char* ox_socket_getipoffd(sock fd);
const char* ox_socket_getipstr(unsigned int ip);
int ox_socket_send(sock fd, const char* buffer, int len);
sock ox_socket_accept(sock listenSocket, struct sockaddr* addr, socklen_t* addrLen);
sock ox_socket_create(int af, int type, int protocol);

#ifdef  __cplusplus
}
#endif

#endif
