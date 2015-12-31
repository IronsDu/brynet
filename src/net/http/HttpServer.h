#ifndef _HTTP_SERVER_H
#define _HTTP_SERVER_H

#include <iostream>
#include <mutex>
#include <string>
#include <map>

#include "WrapTCPService.h"
#include "HttpParser.h"

class HttpServer
{
public:
    typedef std::function < void(const HTTPParser&, TCPSession::PTR, const char* websocketPacket, size_t websocketPacketLen) > HTTPPARSER_CALLBACK;
    typedef std::function < void(TCPSession::PTR) > ENTER_CALLBACK;
    typedef std::function < void(TCPSession::PTR) > CLOSE_CALLBACK;

    HttpServer();
    ~HttpServer();
    WrapServer::PTR         getServer();
    void                    setRequestHandle(HTTPPARSER_CALLBACK requestCallback);

    void                    addConnection(int fd, ENTER_CALLBACK enterCallback, HTTPPARSER_CALLBACK responseCallback, CLOSE_CALLBACK closeCallback = nullptr);

    void                    startWorkThread(int workthreadnum, TcpService::FRAME_CALLBACK callback = nullptr);
    void                    startListen(int port, const char *certificate = nullptr, const char *privatekey = nullptr);
private:
    void                    setSessionCallback(TCPSession::PTR session, HTTPPARSER_CALLBACK callback, CLOSE_CALLBACK closeCallback = nullptr);
private:
    HTTPPARSER_CALLBACK     mOnRequestCallback;
    WrapServer::PTR         mServer;
    ListenThread::PTR       mListenThread;
};

#endif