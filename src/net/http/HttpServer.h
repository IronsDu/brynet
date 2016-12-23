#ifndef DODO_NET_HTTPSERVER_H_
#define DODO_NET_HTTPSERVER_H_

#include "WrapTCPService.h"
#include "HttpParser.h"
#include "NonCopyable.h"
#include "WebSocketFormat.h"

namespace dodo
{
    namespace net
    {
        class HttpServer;

        class HttpSession final : public NonCopyable
        {
        public:
            typedef std::shared_ptr<HttpSession>    PTR;

            typedef std::function < void(const HTTPParser&, HttpSession::PTR) > HTTPPARSER_CALLBACK;
            typedef std::function < void(HttpSession::PTR, WebSocketFormat::WebSocketFrameType opcode, const std::string& payload) > WS_CALLBACK;

            typedef std::function < void(HttpSession::PTR) > CLOSE_CALLBACK;

            HttpSession(TCPSession::PTR);

            void                    setHttpCallback(HTTPPARSER_CALLBACK&& callback);
            void                    setCloseCallback(CLOSE_CALLBACK&& callback);
            void                    setWSCallback(WS_CALLBACK&& callback);

            void                    setHttpCallback(const HTTPPARSER_CALLBACK& callback);
            void                    setCloseCallback(const CLOSE_CALLBACK& callback);
            void                    setWSCallback(const WS_CALLBACK& callback);

            HTTPPARSER_CALLBACK&    getHttpCallback();
            CLOSE_CALLBACK&         getCloseCallback();
            WS_CALLBACK&            getWSCallback();

            void                    send(const DataSocket::PACKET_PTR& packet, const DataSocket::PACKED_SENDED_CALLBACK& callback = nullptr);
            void                    send(const char* packet, size_t len, const DataSocket::PACKED_SENDED_CALLBACK& callback = nullptr);

            void                    postShutdown() const;
            void                    postClose() const;

            int64_t                 getUD() const;
            void                    setUD(int64_t userData);

        private:
            TCPSession::PTR&        getSession();

        private:
            TCPSession::PTR         mSession;
            int64_t                 mUserData;
            HTTPPARSER_CALLBACK     mHttpRequestCallback;
            WS_CALLBACK             mWSCallback;
            CLOSE_CALLBACK          mCloseCallback;

            friend class HttpServer;
        };

        class HttpServer : public NonCopyable
        {
        public:
            typedef std::shared_ptr<HttpServer> PTR;

            typedef std::function < void(HttpSession::PTR&) > ENTER_CALLBACK;

            HttpServer();
            ~HttpServer();
            WrapServer::PTR         getServer();

            void                    setEnterCallback(const ENTER_CALLBACK& callback);

            void                    addConnection(sock fd, 
                                                    const ENTER_CALLBACK& enterCallback,
                                                    const HttpSession::HTTPPARSER_CALLBACK& responseCallback,
                                                    const HttpSession::WS_CALLBACK& wsCallback = nullptr,
                                                    const HttpSession::CLOSE_CALLBACK& closeCallback = nullptr);

            void                    startWorkThread(size_t workthreadnum, TcpService::FRAME_CALLBACK callback = nullptr);
            void                    startListen(bool isIPV6, const std::string& ip, int port, const char *certificate = nullptr, const char *privatekey = nullptr);
        private:
            void                    setSessionCallback(HttpSession::PTR& httpSession, 
                                                        const HttpSession::HTTPPARSER_CALLBACK& callback, 
                                                        const HttpSession::WS_CALLBACK& wsCallback = nullptr, 
                                                        const HttpSession::CLOSE_CALLBACK& closeCallback = nullptr);

        private:
            ENTER_CALLBACK          mOnEnter;
            WrapServer::PTR         mServer;
            ListenThread::PTR       mListenThread;
        };
    }
}

#endif