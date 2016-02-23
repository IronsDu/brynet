#include <string>
using namespace std;

#include "SHA1.h"
#include "base64.h"
#include "http_parser.h"
#include "WebSocketFormat.h"
#include "HttpServer.h"

HttpSession::HttpSession(TCPSession::PTR session)
{
    mUserData = -1;
    mSession = session;
}

TCPSession::PTR HttpSession::getSession()
{
    return mSession;
}

int64_t HttpSession::getUD() const
{
    return mUserData;
}

void HttpSession::setUD(int64_t userData)
{
    mUserData = userData;
}

void HttpSession::setRequestCallback(HTTPPARSER_CALLBACK callback)
{
    mRequestCallback = callback;
}

void HttpSession::setCloseCallback(CLOSE_CALLBACK callback)
{
    mCloseCallback = callback;
}

HttpSession::HTTPPARSER_CALLBACK& HttpSession::getRequestCallback()
{
    return mRequestCallback;
}

HttpSession::CLOSE_CALLBACK& HttpSession::getCloseCallback()
{
    return mCloseCallback;
}

HttpServer::HttpServer()
{
    mServer = std::make_shared<WrapServer>();
}

HttpServer::~HttpServer()
{
    if (mListenThread != nullptr)
    {
        mListenThread->closeListenThread();
    }
    if (mServer != nullptr)
    {
        mServer->getService()->closeService();
    }
}

WrapServer::PTR HttpServer::getServer()
{
    return mServer;
}

void HttpServer::setEnterCallback(ENTER_CALLBACK callback)
{
    mOnEnter = callback;
}

void HttpServer::setRequestHandle(HTTPPARSER_CALLBACK requestCallback, CLOSE_CALLBACK closeCallback)
{
    mOnRequestCallback = requestCallback;
    mOnCloseCallback = closeCallback;
}

void HttpServer::addConnection(int fd, ENTER_CALLBACK enterCallback, HTTPPARSER_CALLBACK responseCallback, CLOSE_CALLBACK closeCallback)
{
    mServer->addSession(fd, [this, enterCallback, responseCallback, closeCallback](TCPSession::PTR session){
        HttpSession::PTR httpSession = std::make_shared<HttpSession>(session);
        enterCallback(httpSession);
        setSessionCallback(httpSession, responseCallback, closeCallback);
    }, false, 32*1024 * 1024);
}

void HttpServer::startWorkThread(int workthreadnum, TcpService::FRAME_CALLBACK callback)
{
    mServer->startWorkThread(workthreadnum, callback);
}

void HttpServer::startListen(int port, const char *certificate /* = nullptr */, const char *privatekey /* = nullptr */)
{
    if (mListenThread == nullptr)
    {
        mListenThread = std::make_shared<ListenThread>();
        mListenThread->startListen(port, certificate, privatekey, [this](int fd){
            mServer->addSession(fd, [this](TCPSession::PTR session){
                HttpSession::PTR httpSession = std::make_shared<HttpSession>(session);
                if (mOnEnter != nullptr)
                {
                    mOnEnter(httpSession);
                }
                setSessionCallback(httpSession, httpSession->getRequestCallback(), httpSession->getCloseCallback());
            }, false, 32 * 1024 * 1024);
        });
    }
}

void HttpServer::setSessionCallback(HttpSession::PTR httpSession, HTTPPARSER_CALLBACK callback, CLOSE_CALLBACK closeCallback)
{
    /*TODO::keep alive and timeout close */
    TCPSession::PTR session = httpSession->getSession();
    HTTPParser* httpParser = new HTTPParser(HTTP_BOTH);
    session->setUD((int64_t)httpParser);
    session->setCloseCallback([closeCallback, httpSession](TCPSession::PTR session){
        HTTPParser* httpParser = (HTTPParser*)session->getUD();
        delete httpParser;
        if (closeCallback != nullptr)
        {
            closeCallback(httpSession);
        }
    });
    session->setDataCallback([this, callback, httpSession](TCPSession::PTR session, const char* buffer, size_t len){
        int retlen = 0;
        HTTPParser* httpParser = (HTTPParser*)session->getUD();
        if (httpParser->isWebSocket())
        {
            std::string frame(buffer, len);
            std::string payload;
            uint8_t opcode; /*TODO::opcode是否回传给回调函数*/
            if (WebSocketFormat::wsFrameExtract(frame, payload, opcode))
            {
                callback(*httpParser, httpSession, payload.c_str(), payload.size());
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
                    callback(*httpParser, httpSession, nullptr, 0);
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