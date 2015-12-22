#include <string>
using namespace std;

#include "SHA1.h"
#include "base64.h"
#include "http_parser.h"
#include "WebSocketFormat.h"
#include "HttpServer.h"

HttpServer::HttpServer()
{
    mServer = std::make_shared<WrapServer>();
}

void HttpServer::setRequestHandle(HTTPPARSER_CALLBACK requestCallback)
{
    mOnRequestCallback = requestCallback;
}

void HttpServer::addConnection(int fd, ENTER_CALLBACK enterCallback, HTTPPARSER_CALLBACK responseCallback)
{
    mServer->addSession(fd, [this, enterCallback, responseCallback](TCPSession::PTR session){
        enterCallback(session);
        setSessionCallback(session, responseCallback);
    }, false, 32*1024 * 1024);
}

void HttpServer::start(int port, int workthreadnum, const char *certificate, const char *privatekey)
{
    if (mListenThread == nullptr)
    {
        mListenThread = std::make_shared<ListenThread>();
    }

    mListenThread->startListen(port, certificate, privatekey, [this](int fd){
        mServer->addSession(fd, [this](TCPSession::PTR session){
            setSessionCallback(session, mOnRequestCallback);
        }, false, 32*1024*1024);
    });
    mServer->startWorkThread(workthreadnum);
}

void HttpServer::setSessionCallback(TCPSession::PTR session, HTTPPARSER_CALLBACK callback)
{
    /*TODO::keep alive and timeout close */
    HTTPParser* httpParser = new HTTPParser(HTTP_BOTH);
    session->setUD((int64_t)httpParser);
    session->setCloseCallback([](TCPSession::PTR session){
        HTTPParser* httpParser = (HTTPParser*)session->getUD();
        delete httpParser;
    });
    session->setDataCallback([this, callback](TCPSession::PTR session, const char* buffer, int len){
        int retlen = 0;
        HTTPParser* httpParser = (HTTPParser*)session->getUD();
        if (httpParser->isWebSocket())
        {
            std::string frame(buffer, len);
            std::string payload;
            uint8_t opcode; /*TODO::opcode是否回传给回调函数*/
            if (WebSocketFormat::wsFrameExtract(frame, payload, opcode))
            {
                callback(*httpParser, session, payload.c_str(), payload.size());
                retlen = len;
            }
        }
        else
        {
            retlen = httpParser->tryParse(buffer, len);
            if (httpParser->isCompleted())
            {
                if (httpParser->isWebSocket())
                {
                    std::string response = WebSocketFormat::wsHandshake(httpParser->getValue("Sec-WebSocket-Key"));
                    session->send(response.c_str(), response.size());
                }
                else
                {
                    callback(*httpParser, session, nullptr, 0);
                    if (httpParser->isKeepAlive())
                    {
                        /*清除本次http报文数据，为下一次http报文准备*/
                        httpParser->clearParse();
                    }
                }
            }
        }

        return retlen;
    });
}