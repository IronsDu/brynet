#ifndef _HTTP_SERVER_H
#define _HTTP_SERVER_H

#include "WrapTCPService.h"
#include "HttpParser.h"
#include "NonCopyable.h"

class HttpServer;

class HttpSession final : public NonCopyable
{
public:
    typedef std::shared_ptr<HttpSession>    PTR;

    typedef std::function < void(const HTTPParser&, HttpSession::PTR, const char* websocketPacket, size_t websocketPacketLen) > HTTPPARSER_CALLBACK;
    typedef std::function < void(HttpSession::PTR) > CLOSE_CALLBACK;

    HttpSession(TCPSession::PTR);

    void                    setRequestCallback(HTTPPARSER_CALLBACK callback);
    void                    setCloseCallback(CLOSE_CALLBACK callback);

    HTTPPARSER_CALLBACK&    getRequestCallback();
    CLOSE_CALLBACK&         getCloseCallback();

    void                    send(const DataSocket::PACKET_PTR& packet, const DataSocket::PACKED_SENDED_CALLBACK& callback = nullptr);
    void                    send(const char* packet, size_t len, const DataSocket::PACKED_SENDED_CALLBACK& callback = nullptr);

    void                    postShutdown() const;
    void                    postClose() const;

    int64_t                 getUD() const;
    void                    setUD(int64_t userData);

private:
    TCPSession::PTR         getSession();

private:
    TCPSession::PTR         mSession;
    int64_t                 mUserData;
    HTTPPARSER_CALLBACK     mRequestCallback;
    CLOSE_CALLBACK          mCloseCallback;

    friend class HttpServer;
};

class HttpServer : public NonCopyable
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