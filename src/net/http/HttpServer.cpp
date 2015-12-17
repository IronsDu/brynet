#include "http_parser.h"
#include "HttpServer.h"

HttpServer::HttpServer()
{
    mServer = std::make_shared<WrapServer>();
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
    if (mListenThread == nullptr)
    {
        mListenThread = std::make_shared<ListenThread>();
    }

    mListenThread->startListen(port, nullptr, nullptr, [this](int fd){
        mServer->addSession(fd, [this](TCPSession::PTR session){
            setSessionCallback(session, mOnRequestCallback);
        }, false);
    });
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
        if (httpProtocol->isCompleted())
        {
            callback(*httpProtocol, session, buffer, len);
            return len;
        }
        else
        {
            int retlen = httpProtocol->appendAndParse(buffer, len);
            if (httpProtocol->isCompleted())
            {
                callback(*httpProtocol, session, nullptr, 0);
            }
            return retlen;
        }
        return 0;
    });
}