#include "NetSession.h"

void WrapAddNetSession(WrapTcpService::PTR service, sock fd, BaseNetSession::PTR connection, int pingCheckTime, size_t maxRecvBufferSize)
{
    service->addSession(fd, [connection, service, pingCheckTime](const TCPSession::PTR& session){
        connection->setSession(service, session->getSocketID(), session->getIP());
        connection->onEnter();

        service->getService()->setPingCheckTime(session->getSocketID(), pingCheckTime);

        session->setCloseCallback([connection](const TCPSession::PTR& session){
            connection->onClose();
        });

        session->setDataCallback([connection](const TCPSession::PTR& session, const char* buffer, size_t len){
            return connection->onMsg(buffer, len);
        });
    }, false, maxRecvBufferSize);
}