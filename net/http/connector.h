#ifndef _CONNECTOR_H
#define _CONNECTOR_H

#include <list>
using namespace std;

#include "socketlibtypes.h"

class ThreadConnector;

/*  异步链接附加数据基类，仅用于Connector析构函数可以delete ConnectorBaseReq*,而不导致内存泄漏   */
class ConnectorBaseReq
{
public:
    virtual ~ConnectorBaseReq()
    {
    }
};

class Connector
{
public:
    Connector();
    virtual ~Connector();

    /*  发起异步链接，此链接成功后会触发onConnectSuccess虚函数，否则触发onConnectFailed  */
    void            connect(const char* ip, int port, int ms, ConnectorBaseReq* ud);

    void            poll();

private:
    virtual void    onConnectFailed(ConnectorBaseReq* ud) = 0;  /*  链接失败    */
    virtual void    onConnectSuccess(sock fd, ConnectorBaseReq* ud) = 0; /*  链接成功    */

private:

    struct non_connect_req
    {
        ConnectorBaseReq*   ud;         /*  逻辑请求方使用new创建，因为Connector的析构函数会delete ud    */
    };

    /*  请求队列    */
    list<non_connect_req>  m_reqs;

    /*  单独线程connect  */
    ThreadConnector*        m_threadConnector;
};

#endif