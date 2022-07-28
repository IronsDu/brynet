#pragma once

#include <brynet/base/NonCopyable.hpp>
#include <brynet/net/TcpService.hpp>
#include <brynet/net/http/HttpParser.hpp>
#include <brynet/net/http/WebSocketFormat.hpp>
#include <memory>
#include <string>

namespace brynet { namespace net { namespace http {

class HttpService;
class HttpSessionHandlers;

class HttpSession : public brynet::base::NonCopyable
{
public:
    using Ptr = std::shared_ptr<HttpSession>;

    using EnterCallback = std::function<void(const HttpSession::Ptr&, HttpSessionHandlers&)>;
    using HttpHeaderCallback = std::function<void(const HTTPParser&, const HttpSession::Ptr&)>;
    using HttpBodyCallback = std::function<void(const HTTPParser&, const HttpSession::Ptr&, const char*, size_t)>;
    using HttpEndCallback = std::function<void(const HTTPParser&, const HttpSession::Ptr&)>;
    using WsCallback = std::function<void(const HttpSession::Ptr&,
                                          WebSocketFormat::WebSocketFrameType opcode,
                                          const std::string& payload)>;

    using ClosedCallback = std::function<void(const HttpSession::Ptr&)>;
    using WsConnectedCallback = std::function<void(const HttpSession::Ptr&, const HTTPParser&)>;

public:
    template<typename PacketType>
    void send(PacketType&& packet,
              TcpConnection::PacketSendedCallback&& callback = nullptr)
    {
        mSession->send(std::forward<PacketType&&>(packet),
                       std::move(callback));
    }
    void send(const char* packet,
              size_t len,
              TcpConnection::PacketSendedCallback&& callback = nullptr)
    {
        mSession->send(packet, len, std::move(callback));
    }

    void postShutdown() const
    {
        mSession->postShutdown();
    }

    void postClose() const
    {
        mSession->postDisConnect();
    }

    const std::string& getIP() const
    {
        return mSession->getIP();
    }

protected:
    explicit HttpSession(TcpConnection::Ptr session)
    {
        mSession = std::move(session);
    }

    virtual ~HttpSession() = default;

    static Ptr Create(TcpConnection::Ptr session)
    {
        class make_shared_enabler : public HttpSession
        {
        public:
            explicit make_shared_enabler(TcpConnection::Ptr session)
                : HttpSession(std::move(session))
            {}
        };
        return std::make_shared<make_shared_enabler>(std::move(session));
    }

    const TcpConnection::Ptr& getSession() const
    {
        return mSession;
    }

    const ClosedCallback& getCloseCallback() const
    {
        return mCloseCallback;
    }

    const WsCallback& getWSCallback() const
    {
        return mWSCallback;
    }

    const WsConnectedCallback& getWSConnectedCallback() const
    {
        return mWSConnectedCallback;
    }

private:
    void setClosedCallback(ClosedCallback&& callback)
    {
        mCloseCallback = std::move(callback);
    }

    void setWSCallback(WsCallback&& callback)
    {
        mWSCallback = std::move(callback);
    }

    void setWSConnected(WsConnectedCallback&& callback)
    {
        mWSConnectedCallback = std::move(callback);
    }

private:
    TcpConnection::Ptr mSession;
    WsCallback mWSCallback;
    ClosedCallback mCloseCallback;
    WsConnectedCallback mWSConnectedCallback;

    friend class HttpService;
};

class HttpSessionHandlers
{
public:
    void setHeaderCallback(HttpSession::HttpHeaderCallback&& callback)
    {
        mHttpHeaderCallback = std::move(callback);
    }

    // if call setHttpBodyCallback, the http body need user manager.
    // so, you can't use body from HttpParser object in HttpEndCallback.
    void setHttpBodyCallback(HttpSession::HttpBodyCallback&& callback)
    {
        mHttpBodyCallback = std::move(callback);
    }

    void setHttpEndCallback(HttpSession::HttpEndCallback&& callback)
    {
        mHttpEndCallback = std::move(callback);
    }

    void setClosedCallback(HttpSession::ClosedCallback&& callback)
    {
        mCloseCallback = std::move(callback);
    }

    void setWSCallback(HttpSession::WsCallback&& callback)
    {
        mWSCallback = std::move(callback);
    }

    void setWSConnected(HttpSession::WsConnectedCallback&& callback)
    {
        mWSConnectedCallback = std::move(callback);
    }

private:
    HttpSession::HttpHeaderCallback mHttpHeaderCallback;
    HttpSession::HttpBodyCallback mHttpBodyCallback;
    HttpSession::HttpEndCallback mHttpEndCallback;
    HttpSession::WsCallback mWSCallback;
    HttpSession::ClosedCallback mCloseCallback;
    HttpSession::WsConnectedCallback mWSConnectedCallback;

    friend class HttpService;
};

class HttpService
{
public:
    static void setup(const TcpConnection::Ptr& session,
                      const HttpSession::EnterCallback& enterCallback)
    {
        auto httpParser = std::make_shared<HTTPParser>(HTTP_BOTH);

        auto httpSession = HttpSession::Create(session);
        if (enterCallback != nullptr)
        {
            HttpSessionHandlers handlers;
            enterCallback(httpSession, handlers);
            httpSession->setClosedCallback(std::move(handlers.mCloseCallback));
            httpSession->setWSCallback(std::move(handlers.mWSCallback));
            httpSession->setWSConnected(std::move(handlers.mWSConnectedCallback));

            auto headerCB = handlers.mHttpHeaderCallback;
            if (headerCB != nullptr)
            {
                httpParser->setHeaderCallback([=]() {
                    headerCB(*httpParser, httpSession);
                });
            }

            auto bodyCB = handlers.mHttpBodyCallback;
            if (bodyCB != nullptr)
            {
                httpParser->setBodyCallback([=](const char* body, size_t length) {
                    bodyCB(*httpParser, httpSession, body, length);
                });
            }

            auto endCB = handlers.mHttpEndCallback;
            if (endCB != nullptr)
            {
                httpParser->setEndCallback([=]() {
                    endCB(*httpParser, httpSession);
                });
            }
        }
        HttpService::handle(httpSession, httpParser);
    }

private:
    static void handle(const HttpSession::Ptr& httpSession, HTTPParser::Ptr httpParser)
    {
        /*TODO::keep alive and timeout close */
        auto& session = httpSession->getSession();

        session->setDisConnectCallback([httpSession, httpParser](const TcpConnection::Ptr&) {
            if (!httpParser->isCompleted())
            {
                // try pass EOF to http parser
                HttpService::ProcessHttp(nullptr, 0, httpParser, httpSession);
            }

            const auto& cb = httpSession->getCloseCallback();
            if (cb != nullptr)
            {
                cb(httpSession);
            }
        });

        session->setDataCallback([httpSession, httpParser](
                                         brynet::base::BasePacketReader& reader) {
            size_t retLen = 0;

            if (httpParser->isWebSocket())
            {
                retLen = HttpService::ProcessWebSocket(reader.begin(),
                                                       reader.size(),
                                                       httpParser,
                                                       httpSession);
            }
            else if (httpParser->isUpgrade())
            {
                // TODO::not support other upgrade protocol
            }
            else
            {
                retLen = HttpService::ProcessHttp(reader.begin(),
                                                  reader.size(),
                                                  httpParser,
                                                  httpSession);
                // if http_parser_execute not consume all data that indicate cause error in parser.
                // so we need close connection.
                if (retLen != reader.size())
                {
                    httpSession->postClose();
                }
            }

            reader.addPos(retLen);
            reader.savePos();
        });
    }

    static size_t ProcessWebSocket(const char* buffer,
                                   size_t len,
                                   const HTTPParser::Ptr& httpParser,
                                   const HttpSession::Ptr& httpSession)
    {
        size_t leftLen = len;

        const auto& wsCallback = httpSession->getWSCallback();
        auto& cacheFrame = httpParser->getWSCacheFrame();
        auto& parseString = httpParser->getWSParseString();

        while (leftLen > 0)
        {
            parseString.clear();

            auto opcode = WebSocketFormat::WebSocketFrameType::ERROR_FRAME;
            size_t frameSize = 0;
            bool isFin = false;

            if (!WebSocketFormat::wsFrameExtractBuffer(buffer,
                                                       leftLen,
                                                       parseString,
                                                       opcode,
                                                       frameSize,
                                                       isFin))
            {
                // 如果没有解析出完整的ws frame则退出函数
                break;
            }

            // 如果当前fram的fin为false或者opcode为延续包
            // 则将当前frame的payload添加到cache
            if (!isFin ||
                opcode == WebSocketFormat::WebSocketFrameType::CONTINUATION_FRAME)
            {
                cacheFrame += parseString;
                parseString.clear();
            }
            // 如果当前fram的fin为false，并且opcode不为延续包
            // 则表示收到分段payload的第一个段(frame)，需要缓存当前frame的opcode
            if (!isFin &&
                opcode != WebSocketFormat::WebSocketFrameType::CONTINUATION_FRAME)
            {
                httpParser->cacheWSFrameType(opcode);
            }

            leftLen -= frameSize;
            buffer += frameSize;

            if (!isFin)
            {
                continue;
            }

            // 如果fin为true，并且opcode为延续包
            // 则表示分段payload全部接受完毕
            // 因此需要获取之前第一次收到分段frame的opcode作为整个payload的类型
            if (opcode == WebSocketFormat::WebSocketFrameType::CONTINUATION_FRAME)
            {
                if (!cacheFrame.empty())
                {
                    parseString = std::move(cacheFrame);
                    cacheFrame.clear();
                }
                opcode = httpParser->getWSFrameType();
            }

            if (wsCallback != nullptr)
            {
                wsCallback(httpSession, opcode, parseString);
            }
        }

        return (len - leftLen);
    }

    static size_t ProcessHttp(const char* buffer,
                              size_t len,
                              const HTTPParser::Ptr& httpParser,
                              const HttpSession::Ptr& httpSession)
    {
        size_t retlen = len;
        if (!httpParser->isCompleted())
        {
            retlen = httpParser->tryParse(buffer, len);
            if (!httpParser->isCompleted())
            {
                return retlen;
            }
        }

        if (httpParser->isWebSocket())
        {
            if (httpParser->hasKey("Sec-WebSocket-Key"))
            {
                auto response = WebSocketFormat::wsHandshake(
                        httpParser->getValue("Sec-WebSocket-Key"));
                httpSession->send(response.c_str(),
                                  response.size());
            }

            const auto& wsConnectedCallback = httpSession->getWSConnectedCallback();
            if (wsConnectedCallback != nullptr)
            {
                wsConnectedCallback(httpSession, *httpParser);
            }
        }

        return retlen;
    }
};

}}}// namespace brynet::net::http
