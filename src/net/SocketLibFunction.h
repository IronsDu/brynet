#ifndef _SOCKETLIBFUNCTION_H_INCLUDED_
#define _SOCKETLIBFUNCTION_H_INCLUDED_

#include <stdbool.h>
#include "SocketLibTypes.h"
#include "dllconf.h"

#ifdef  __cplusplus
extern "C" {
#endif

DLL_CONF    bool ox_socket_init();
DLL_CONF    void ox_socket_destroy();
DLL_CONF    int  ox_socket_nodelay(sock fd);
DLL_CONF    bool ox_socket_nonblock(sock fd);
DLL_CONF    int  ox_socket_setsdsize(sock fd, int sd_size);
DLL_CONF    int  ox_socket_setrdsize(sock fd, int rd_size);
DLL_CONF    sock ox_socket_connect(const char* server_ip, int port);
DLL_CONF    sock ox_socket_nonblockconnect(const char* server_ip, int port, int timeout);
DLL_CONF    sock ox_socket_listen(int port, int back_num);
DLL_CONF    void ox_socket_close(sock fd);
DLL_CONF    const char* ox_socket_getipoffd(sock fd);
DLL_CONF    const char* ox_socket_getipstr(unsigned int ip);
DLL_CONF    int ox_socket_send(sock fd, const char* buffer, int len);

#ifdef  __cplusplus
}
#endif

#endif
