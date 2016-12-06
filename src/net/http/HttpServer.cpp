#include <string>
#include <cstring>

#include "SHA1.h"
#include "base64.h"
#include "http_parser.h"
#include "WebSocketFormat.h"

#include "HttpServer.h"

using namespace dodo::net;

HttpSession::HttpSession(TCPSession::PTR session)
{
    mUserData = -1;
    mSession = session;
}

TCPSession::PTR& HttpSession::getSession()
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

void HttpSession::setHttpCallback(HTTPPARSER_CALLBACK&& callback)
{
    mHttpRequestCallback = std::move(callback);
}

void HttpSession::setCloseCallback(CLOSE_CALLBACK&& callback)
{
    mCloseCallback = std::move(callback);
}

void HttpSession::setWSCallback(WS_CALLBACK&& callback)
{
    mWSCallback = std::move(callback);
}

void HttpSession::setHttpCallback(const HTTPPARSER_CALLBACK& callback)
{
    mHttpRequestCallback = callback;
}

void HttpSession::setCloseCallback(const CLOSE_CALLBACK& callback)
{
    mCloseCallback = callback;
}

void HttpSession::setWSCallback(const WS_CALLBACK& callback)
{
    mWSCallback = callback;
}

HttpSession::HTTPPARSER_CALLBACK& HttpSession::getHttpCallback()
{
    return mHttpRequestCallback;
}

HttpSession::CLOSE_CALLBACK& HttpSession::getCloseCallback()
{
    return mCloseCallback;
}

HttpSession::WS_CALLBACK& HttpSession::getWSCallback()
{
    return mWSCallback;
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

void HttpServer::setEnterCallback(const ENTER_CALLBACK& callback)
{
    mOnEnter = callback;
}

void HttpServer::addConnection(sock fd, 
    const ENTER_CALLBACK& enterCallback, 
    const HttpSession::HTTPPARSER_CALLBACK& responseCallback, 
    const HttpSession::WS_CALLBACK& wsCallback, 
    const HttpSession::CLOSE_CALLBACK& closeCallback)
{
    mServer->addSession(fd, [this, enterCallback, responseCallback, wsCallback, closeCallback](TCPSession::PTR& session){
        HttpSession::PTR httpSession = std::make_shared<HttpSession>(session);
        enterCallback(httpSession);
        setSessionCallback(httpSession, responseCallback, wsCallback, closeCallback);
    }, false, 32*1024 * 1024);
}

void HttpServer::startWorkThread(int workthreadnum, TcpService::FRAME_CALLBACK callback)
{
    mServer->startWorkThread(workthreadnum, callback);
}

void HttpServer::startListen(bool isIPV6, const std::string& ip, int port, const char *certificate /* = nullptr */, const char *privatekey /* = nullptr */)
{
    if (mListenThread == nullptr)
    {
        mListenThread = std::make_shared<ListenThread>();
        mListenThread->startListen(isIPV6, ip, port, certificate, privatekey, [this](sock fd){
            mServer->addSession(fd, [this](TCPSession::PTR& session){
                HttpSession::PTR httpSession = std::make_shared<HttpSession>(session);
                if (mOnEnter != nullptr)
                {
                    mOnEnter(httpSession);
                }
                setSessionCallback(httpSession, httpSession->getHttpCallback(), httpSession->getWSCallback(), httpSession->getCloseCallback());
            }, false, 32 * 1024 * 1024);
        });
    }
}

void HttpServer::setSessionCallback(HttpSession::PTR& httpSession, 
    const HttpSession::HTTPPARSER_CALLBACK& httpCallback,
    const HttpSession::WS_CALLBACK& wsCallback, 
    const HttpSession::CLOSE_CALLBACK& closeCallback)
{
    /*TODO::keep alive and timeout close */
    TCPSession::PTR& session = httpSession->getSession();
    HTTPParser* httpParser = new HTTPParser(HTTP_BOTH);
    session->setUD((int64_t)httpParser);
    session->setCloseCallback([closeCallback, httpSession](TCPSession::PTR& session){
        HTTPParser* httpParser = (HTTPParser*)session->getUD();
        delete httpParser;
        if (closeCallback != nullptr)
        {
            closeCallback(httpSession);
        }
    });
    session->setDataCallback([this, httpCallback, wsCallback, httpSession](TCPSession::PTR& session, const char* buffer, size_t len){
        size_t retlen = 0;
        HTTPParser* httpParser = (HTTPParser*)session->getUD();
        if (httpParser->isWebSocket())
        {
            const char* parse_str = buffer;
            size_t leftLen = len;

            while (leftLen > 0)
            {
                std::string payload;
                auto opcode = WebSocketFormat::WebSocketFrameType::ERROR_FRAME; /*TODO::opcode是否回传给回调函数*/
                size_t frameSize = 0;
                bool isFin = false;
                if (WebSocketFormat::wsFrameExtractBuffer(parse_str, leftLen, payload, opcode, frameSize, isFin))
                {
                    if (isFin && (opcode == WebSocketFormat::WebSocketFrameType::TEXT_FRAME || opcode == WebSocketFormat::WebSocketFrameType::BINARY_FRAME))
                    {
                        if (!httpParser->getWSCacheFrame().empty())
                        {
                            payload.insert(0, httpParser->getWSCacheFrame().c_str(), httpParser->getWSCacheFrame().size());
                            httpParser->getWSCacheFrame().clear();
                        }

                        assert(wsCallback != nullptr);
                        if (wsCallback != nullptr)
                        {
                            wsCallback(httpSession, opcode, payload);
                        }
                    }
                    else if (opcode == WebSocketFormat::WebSocketFrameType::CONTINUATION_FRAME)
                    {
                        httpParser->getWSCacheFrame().append(payload.c_str(), payload.size());
                    }
                    else if (opcode == WebSocketFormat::WebSocketFrameType::PING_FRAME ||
                            opcode == WebSocketFormat::WebSocketFrameType::PONG_FRAME ||
                            opcode == WebSocketFormat::WebSocketFrameType::CLOSE_FRAME)
                    {
                        assert(wsCallback != nullptr);
                        if (wsCallback != nullptr)
                        {
                            wsCallback(httpSession, opcode, payload);
                        }
                    }
                    else
                    {
                        assert(false);
                    }

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
                    assert(httpCallback != nullptr);
                    if (httpCallback != nullptr)
                    {
                        httpCallback(*httpParser, httpSession);
                    }
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
