#include <assert.h>

#include "socketlibfunction.h"
#include "socketlibtypes.h"
#include "httprequest.h"
#include "timeaction.h"
#include "systemlib.h"
#include "thread_reactor.h"
#include "cpp_server.h"

#include "webclient.h"

HttpConnector::HttpConnector(CppServer* server)
{
    ox_socket_init();
    m_server = server;
}

ConnectorBaseReq* HttpConnector::popRequestUD()
{
    ConnectorBaseReq* ret = NULL;
    if(!m_requestDatas.empty())
    {
        ret = *(m_requestDatas.begin());
        m_requestDatas.pop_front();
    }

    return ret;
}

void HttpConnector::onConnectSuccess(sock fd, ConnectorBaseReq* ud)
{
    m_requestDatas.push_back(ud);
    m_server->addFD(fd);
}

void HttpConnector::onConnectFailed(ConnectorBaseReq* ud)
{
    delete ud;
}