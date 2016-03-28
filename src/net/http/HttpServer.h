#ifndef _HTTP_SERVER_H
#define _HTTP_SERVER_H

#include "WrapTCPService.h"
#include "HttpParser.h"

class HttpSession
{
public:
    typedef std::shared_ptr<HttpSession>    PTR;

    typedef std::function < void(const HTTPParser&, HttpSession::PTR, const char* websocketPacket, size_t websocketPacketLen) > HTTPPARSER_CALLBACK;
    typedef std::function < void(HttpSession::PTR) > CLOSE_CALLBACK;

    HttpSession(TCPSession::PTR);
    /*TODO::返回它之后不太安全,因为能访问TCPSession的特定函数：setUD，然而此函数应该只能在此http封装层调用*/
    TCPSession::PTR     getSession();
    int64_t             getUD() const;
    void                setUD(int64_t userData);

    void                    setRequestCallback(HTTPPARSER_CALLBACK callback);
    void                    setCloseCallback(CLOSE_CALLBACK callback);

    HTTPPARSER_CALLBACK&    getRequestCallback();
    CLOSE_CALLBACK&         getCloseCallback();

private:
    TCPSession::PTR     mSession;
    int64_t             mUserData;
    HTTPPARSER_CALLBACK mRequestCallback;
    CLOSE_CALLBACK      mCloseCallback;
};

class HttpServer
{
public:
    typedef std::shared_ptr<HttpServer> PTR;

    typedef std::function < void(HttpSession::PTR) > ENTER_CALLBACK;

    HttpServer();
    ~HttpServer();
    WrapServer::PTR         getServer();

    void                    setEnterCallback(ENTER_CALLBACK callback);

    void                    addConnection(int fd, ENTER_CALLBACK enterCallback, HttpSession::HTTPPARSER_CALLBACK responseCallback, HttpSession::CLOSE_CALLBACK closeCallback = nullptr);

    void                    startWorkThread(int workthreadnum, TcpService::FRAME_CALLBACK callback = nullptr);
    void                    startListen(bool isIPV6, std::string ip, int port, const char *certificate = nullptr, const char *privatekey = nullptr);
private:
    void                    setSessionCallback(HttpSession::PTR httpSession, HttpSession::HTTPPARSER_CALLBACK callback, HttpSession::CLOSE_CALLBACK closeCallback = nullptr);
private:
    ENTER_CALLBACK          mOnEnter;
    WrapServer::PTR         mServer;
    ListenThread::PTR       mListenThread;
};

#endif