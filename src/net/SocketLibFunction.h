#ifndef _SOCKETLIBFUNCTION_H_INCLUDED_
#define _SOCKETLIBFUNCTION_H_INCLUDED_

#include <stdbool.h>
#include "SocketLibTypes.h"

#ifdef  __cplusplus
extern "C" {
#endif

bool ox_socket_init();
void ox_socket_destroy();
int  ox_socket_nodelay(sock fd);
bool ox_socket_nonblock(sock fd);
int  ox_socket_setsdsize(sock fd, int sd_size);
int  ox_socket_setrdsize(sock fd, int rd_size);
sock ox_socket_connect(const char* server_ip, int port);
sock ox_socket_nonblockconnect(const char* server_ip, int port, int timeout);
sock ox_socket_listen(int port, int back_num);
void ox_socket_close(sock fd);
const char* ox_socket_getipoffd(sock fd);
const char* ox_socket_getipstr(unsigned int ip);
int ox_socket_send(sock fd, const char* buffer, int len);

#ifdef  __cplusplus
}
#endif

#endif
