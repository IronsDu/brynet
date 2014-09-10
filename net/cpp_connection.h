#ifndef _CPP_CONNECTION_H
#define _CPP_CONNECTION_H

#include <stdint.h>
#include <functional>

#include "connection.h"

class NetConnection
{
    typedef std::function<int(const char*, int)>        PACKET_CHECK;
    typedef std::function<void(struct msg_data_s*)>     PACKET_HANDLE;

public:
    NetConnection();

    void            create(int rdsize, int sdsize);

    void            setCheckPacketHandle(PACKET_CHECK callback);
    void            setMsgHandle(PACKET_HANDLE callback);

    void            logicPoll(int64_t ms);
    void            netPoll(int64_t ms);

    void            sendDisconnect();
    void            sendConnect(const char* ip, int port, int timeout);
    void            sendData(const char* data, int len);

private:
    static  int     s_check(const char* buffer, int len, void* ext);
    static  void    s_handle(struct connection_s* self, struct msg_data_s*, void* ext);

private:
    struct connection_s*    m_connection;

    PACKET_CHECK    m_check;
    PACKET_HANDLE   m_handle;
};

#endif