#include <string>
#include <string.h>
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

void HttpSession::send(const DataSocket::PACKET_PTR& packet, const DataSocket::PACKED_SENDED_CALLBACK& callback /* = nullptr */)
{
    mSession->send(packet, callback);
}

void HttpSession::send(const char* packet, size_t len, const DataSocket::PACKED_SENDED_CALLBACK& callback)
{
    mSession->send(packet, len, callback);
}

void HttpSession::postShutdown() const
{
    mSession->postShutdown();
}

void HttpSession::postClose() const
{
    mSession->postClose();
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

void HttpServer::addConnection(int fd, ENTER_CALLBACK enterCallback, HttpSession::HTTPPARSER_CALLBACK responseCallback, HttpSession::CLOSE_CALLBACK closeCallback)
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

void HttpServer::startListen(bool isIPV6, std::string ip, int port, const char *certificate /* = nullptr */, const char *privatekey /* = nullptr */)
{
    if (mListenThread == nullptr)
    {
        mListenThread = std::make_shared<ListenThread>();
        mListenThread->startListen(isIPV6, ip, port, certificate, privatekey, [this](int fd){
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

void HttpServer::setSessionCallback(HttpSession::PTR httpSession, HttpSession::HTTPPARSER_CALLBACK callback, HttpSession::CLOSE_CALLBACK closeCallback)
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
        size_t retlen = 0;
        HTTPParser* httpParser = (HTTPParser*)session->getUD();
        if (httpParser->isWebSocket())
        {
            const char* parse_str = buffer;
            size_t leftLen = len;

            while (leftLen > 0)
            {
                std::string payload;
                uint8_t opcode; /*TODO::opcode是否回传给回调函数*/
                int frameSize = 0;
                if (WebSocketFormat::wsFrameExtractBuffer(parse_str, leftLen, payload, opcode, frameSize))
                {
                    callback(*httpParser, httpSession, payload.c_str(), payload.size());

                    leftLen -= frameSize;
                    retlen += frameSize;
                    parse_str += frameSize;
                }
                else
                {
                    break;
                }
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
