#include <brynet/net/NetSession.h>

namespace brynet::net
{
    void WrapAddNetSession(WrapTcpService::PTR service,
        sock fd,
        BaseNetSession::PTR connection,
        std::chrono::nanoseconds heartBeat,
        size_t maxRecvBufferSize)
    {
        service->addSession(fd, [connection, service, heartBeat](const TCPSession::PTR& session) {
            connection->setSession(service, session, session->getIP());
            connection->onEnter();

            session->setHeartBeat(heartBeat);

            session->setDisConnectCallback([connection](const TCPSession::PTR& session) {
                connection->onClose();
            });

            session->setDataCallback([connection](const TCPSession::PTR& session, const char* buffer, size_t len) {
                return connection->onMsg(buffer, len);
            });
        }, false, nullptr, maxRecvBufferSize);
    }
}
