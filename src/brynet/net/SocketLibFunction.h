#pragma once

#include <cstdbool>
#include <string>

#include <brynet/net/SocketLibTypes.h>

namespace brynet { namespace net { namespace base {

    bool InitSocket();
    void DestroySocket();

    int  SocketNodelay(sock fd);
    bool SocketBlock(sock fd);
    bool SocketNonblock(sock fd);
    int  SocketSetSendSize(sock fd, int sd_size);
    int  SocketSetRecvSize(sock fd, int rd_size);
    sock Connect(bool isIPV6, const std::string& server_ip, int port);
    sock Listen(bool isIPV6, const char* ip, int port, int back_num);
    void SocketClose(sock fd);
    std::string GetIPOfSocket(sock fd);
    int SocketSend(sock fd, const char* buffer, int len);
    sock Accept(sock listenSocket, struct sockaddr* addr, socklen_t* addrLen);
    sock SocketCreate(int af, int type, int protocol);
	bool IsSelfConnect(sock fd);

} } }
