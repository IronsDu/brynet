#include <string>
#include <brynet/net/http/WebSocketFormat.h>

#include <brynet/net/http/HttpService.h>

namespace brynet { namespace net { namespace http {

    HttpSession::HttpSession(TcpConnection::Ptr session)
    {
        mSession = std::move(session);
    }

    const TcpConnection::Ptr& HttpSession::getSession() const
    {
        return mSession;
    }

    const BrynetAny& HttpSession::getUD() const
    {
        return mUD;
    }

    void HttpSession::setUD(BrynetAny ud)
    {
        mUD = std::move(ud);
    }

    void HttpSession::setHttpCallback(HttpParserCallback&& callback)
    {
        mHttpRequestCallback = std::forward<HttpParserCallback>(callback);
    }

    void HttpSession::setClosedCallback(ClosedCallback&& callback)
    {
        mCloseCallback = std::forward<ClosedCallback>(callback);
    }

    void HttpSession::setWSCallback(WsCallback&& callback)
    {
        mWSCallback = std::forward<WsCallback>(callback);
    }

    void HttpSession::setWSConnected(WsConnectedCallback&& callback)
    {
        mWSConnectedCallback = std::forward<WsConnectedCallback>(callback);
    }

    const HttpSession::HttpParserCallback& HttpSession::getHttpCallback() const
    {
        return mHttpRequestCallback;
    }

    const HttpSession::ClosedCallback& HttpSession::getCloseCallback() const
    {
        return mCloseCallback;
    }

    const HttpSession::WsCallback& HttpSession::getWSCallback() const
    {
        return mWSCallback;
    }

    const HttpSession::WsConnectedCallback& HttpSession::getWSConnectedCallback() const
    {
        return mWSConnectedCallback;
    }

    void HttpSession::send(const char* packet,
        size_t len,
        TcpConnection::PacketSendedCallback&& callback)
    {
        mSession->send(packet, len, std::forward<TcpConnection::PacketSendedCallback>(callback));
    }

    void HttpSession::postShutdown() const
    {
        mSession->postShutdown();
    }

    void HttpSession::postClose() const
    {
        mSession->postDisConnect();
    }

    HttpSession::Ptr HttpSession::Create(TcpConnection::Ptr session)
    {
        class make_shared_enabler : public HttpSession
        {
        public:
            explicit make_shared_enabler(TcpConnection::Ptr session) : HttpSession(std::move(session))
            {}
        };
        return std::make_shared<make_shared_enabler>(std::move(session));
    }

    void HttpService::setup(const TcpConnection::Ptr& session, const HttpSession::EnterCallback& enterCallback)
    {
        auto httpSession = HttpSession::Create(session);
        if (enterCallback != nullptr)
        {
            enterCallback(httpSession);
        }
        HttpService::handle(httpSession);
    }

    size_t HttpService::ProcessWebSocket(const char* buffer,
                                         const size_t len,
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

            if (!WebSocketFormat::wsFrameExtractBuffer(buffer, leftLen, parseString, opcode, frameSize, isFin))
            {
                // 如果没有解析出完整的ws frame则退出函数
                break;
            }

            // 如果当前fram的fin为false或者opcode为延续包，则将当前frame的payload添加到cache
            if (!isFin || opcode == WebSocketFormat::WebSocketFrameType::CONTINUATION_FRAME)
            {
                cacheFrame += parseString;
                parseString.clear();
            }
            // 如果当前fram的fin为false，并且opcode不为延续包，则表示收到分段payload的第一个段(frame)，需要缓存当前frame的opcode
            if (!isFin && opcode != WebSocketFormat::WebSocketFrameType::CONTINUATION_FRAME)
            {
                httpParser->cacheWSFrameType(opcode);
            }

            leftLen -= frameSize;
            buffer += frameSize;

            if (!isFin)
            {
                continue;
            }

            // 如果fin为true，并且opcode为延续包，则表示分段payload全部接受完毕，因此需要获取之前第一次收到分段frame的opcode作为整个payload的类型
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

    size_t HttpService::ProcessHttp(const char* buffer,
                                    const size_t len,
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
                auto response = WebSocketFormat::wsHandshake(httpParser->getValue("Sec-WebSocket-Key"));
                httpSession->send(response.c_str(), response.size());
            }

            const auto& wsConnectedCallback = httpSession->getWSConnectedCallback();
            if (wsConnectedCallback != nullptr)
            {
                wsConnectedCallback(httpSession, *httpParser);
            }
        }
        else
        {
            const auto& httpCallback = httpSession->getHttpCallback();
            if (httpCallback != nullptr)
            {
                httpCallback(*httpParser, httpSession);
            }
            if (httpParser->isKeepAlive())
            {
                httpParser->clearParse();
            }
        }

        return retlen;
    }

    void HttpService::handle(const HttpSession::Ptr& httpSession)
    {
        /*TODO::keep alive and timeout close */
        auto& session = httpSession->getSession();

        session->setDisConnectCallback([httpSession](const TcpConnection::Ptr&) {
            const auto& tmp = httpSession->getCloseCallback();
            if (tmp != nullptr)
            {
                tmp(httpSession);
            }
        });

        auto httpParser = std::make_shared<HTTPParser>(HTTP_BOTH);
        session->setDataCallback([httpSession, httpParser](
            const char* buffer, size_t len) {
            size_t retlen = 0;

            if (httpParser->isWebSocket())
            {
                retlen = HttpService::ProcessWebSocket(buffer, len, httpParser, httpSession);
            }
            else
            {
                retlen = HttpService::ProcessHttp(buffer, len, httpParser, httpSession);
            }

            return retlen;
        });
    }

} } }
