#include "http_parser.h"
#include "HttpServer.h"

HttpServer::HttpServer()
{
    mServer = std::make_shared<WrapServer>();
    mServer->setDefaultEnterCallback([this](TCPSession::PTR session){
        setSessionCallback(session, mOnRequestCallback);
    });
}

void HttpServer::setRequestHandle(HTTPPROTOCOL_CALLBACK requestCallback)
{
    mOnRequestCallback = requestCallback;
}

void HttpServer::addConnection(int fd, ENTER_CALLBACK enterCallback, HTTPPROTOCOL_CALLBACK responseCallback)
{
    mServer->addSession(fd, [this, enterCallback, responseCallback](TCPSession::PTR session){
        enterCallback(session);
        setSessionCallback(session, responseCallback);
    }, false);
}

void HttpServer::start(int port, int workthreadnum)
{
    mServer->startListen(port);
    mServer->startWorkThread(workthreadnum);
}

void HttpServer::setSessionCallback(TCPSession::PTR session, HTTPPROTOCOL_CALLBACK callback)
{
    HTTPProtocol* httpProtocol = new HTTPProtocol(HTTP_BOTH);
    session->setUD((int64_t)httpProtocol);
    session->setCloseCallback([](TCPSession::PTR session){
        HTTPProtocol* httpProtocol = (HTTPProtocol*)session->getUD();
        delete httpProtocol;
    });
    session->setDataCallback([this, callback](TCPSession::PTR session, const char* buffer, int len){
        HTTPProtocol* httpProtocol = (HTTPProtocol*)session->getUD();
        int retlen = httpProtocol->appendAndParse(buffer, len);
        if (httpProtocol->isCompleted())
        {
            callback(*httpProtocol, session);
        }
        return retlen;
    });
}