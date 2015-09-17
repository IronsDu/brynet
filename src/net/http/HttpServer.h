#ifndef _HTTP_SERVER_H
#define _HTTP_SERVER_H

#include <iostream>
#include <mutex>
#include <string>
#include <map>

#include "WrapTCPService.h"
#include "HttpProtocol.h"

class HttpServer
{
public:
    typedef std::function < void(const HTTPProtocol&, TCPSession::PTR) > HTTPPROTOCOL_CALLBACK;
    typedef std::function < void(TCPSession::PTR) > ENTER_CALLBACK;

    HttpServer();

    void                    setRequestHandle(HTTPPROTOCOL_CALLBACK requestCallback);

    void                    addConnection(int fd, ENTER_CALLBACK enterCallback, HTTPPROTOCOL_CALLBACK responseCallback);

    void                    start(int port, int workthreadnum);
private:
    void                    setSessionCallback(TCPSession::PTR session, HTTPPROTOCOL_CALLBACK callback);
private:
    HTTPPROTOCOL_CALLBACK   mOnRequestCallback;
    WrapServer::PTR         mServer;
};

#endif