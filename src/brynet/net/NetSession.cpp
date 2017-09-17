#include <brynet/net/NetSession.h>

using namespace brynet::net;

void WrapAddNetSession(WrapTcpService::PTR service, 
    sock fd, 
    BaseNetSession::PTR connection, 
    std::chrono::nanoseconds pingCheckTime,
    size_t maxRecvBufferSize)
{
    service->addSession(fd, [connection, service, pingCheckTime](const TCPSession::PTR& session) {
        connection->setSession(service, session, session->getIP());
        connection->onEnter();

        session->setPingCheckTime(pingCheckTime);

        session->setCloseCallback([connection](const TCPSession::PTR& session){
            connection->onClose();
        });

        session->setDataCallback([connection](const TCPSession::PTR& session, const char* buffer, size_t len){
            return connection->onMsg(buffer, len);
        });
    }, false, nullptr, maxRecvBufferSize);
}