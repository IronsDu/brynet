#include "cpp_connection.h"

NetConnection::NetConnection()
{
    m_connection = nullptr;
}

void NetConnection::create(int rdsize, int sdsize)
{
    m_connection = ox_connection_new(rdsize, sdsize, NetConnection::s_check, NetConnection::s_handle, this);
}

void NetConnection::setCheckPacketHandle(PACKET_CHECK callback)
{
    m_check = callback;
}

void NetConnection::setMsgHandle(PACKET_HANDLE callback)
{
    m_handle = callback;
}

void NetConnection::logicPoll(int64_t ms)
{
    ox_connection_logicpoll(m_connection, ms);
}

void NetConnection::netPoll(int64_t ms)
{
    ox_connection_netpoll(m_connection, ms);
}

void NetConnection::sendDisconnect()
{
    ox_connection_send_close(m_connection);
}

void NetConnection::sendConnect(const char* ip, int port, int timeout)
{
    ox_connection_send_connect(m_connection, ip, port, timeout);
}

void NetConnection::sendData(const char* data, int len)
{
    struct msg_data_s* msg = ox_connection_sendmsg_claim(m_connection, len);
    memcpy(msg->data, data, len);
    msg->len = len;

    ox_connection_send(m_connection, msg);
}

int NetConnection::s_check(const char* buffer, int len, void* ext)
{
    int checklen = 0;
    NetConnection* connection = static_cast<NetConnection*>(ext);
    if (connection != nullptr)
    {
        checklen = connection->m_check(buffer, len);
    }

    return checklen;
}

void NetConnection::s_handle(struct connection_s* self, struct msg_data_s* msg, void* ext)
{
    NetConnection* connection = static_cast<NetConnection*>(ext);
    if (connection != nullptr)
    {
        connection->m_handle(msg);
    }
}