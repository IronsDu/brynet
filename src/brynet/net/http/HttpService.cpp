#include <string>
#include <cstring>
#include <cassert>

#include <brynet/utils/SHA1.h>
#include <brynet/utils/base64.h>
#include <brynet/net/http/http_parser.h>
#include <brynet/net/http/WebSocketFormat.h>

#include <brynet/net/http/HttpService.h>

using namespace brynet::net;

HttpSession::HttpSession(TCPSession::PTR session)
{
    mSession = std::move(session);
}

TCPSession::PTR& HttpSession::getSession()
{
    return mSession;
}

const BrynetAny& HttpSession::getUD() const
{
    return mUD;
}

void HttpSession::setUD(BrynetAny ud)
{
    mUD = ud;
}

void HttpSession::setHttpCallback(HTTPPARSER_CALLBACK callback)
{
    mHttpRequestCallback = std::move(callback);
}

void HttpSession::setCloseCallback(CLOSE_CALLBACK callback)
{
    mCloseCallback = std::move(callback);
}

void HttpSession::setWSCallback(WS_CALLBACK callback)
{
    mWSCallback = std::move(callback);
}

void HttpSession::setWSConnected(WS_CONNECTED_CALLBACK callback)
{
    mWSConnectedCallback = std::move(callback);
}

const HttpSession::HTTPPARSER_CALLBACK& HttpSession::getHttpCallback()
{
    return mHttpRequestCallback;
}

const HttpSession::CLOSE_CALLBACK& HttpSession::getCloseCallback()
{
    return mCloseCallback;
}

const HttpSession::WS_CALLBACK& HttpSession::getWSCallback()
{
    return mWSCallback;
}

const HttpSession::WS_CONNECTED_CALLBACK& HttpSession::getWSConnectedCallback()
{
    return mWSConnectedCallback;
}

void HttpSession::send(const DataSocket::PACKET_PTR& packet, 
    const DataSocket::PACKED_SENDED_CALLBACK& callback /* = nullptr */)
{
    mSession->send(packet, callback);
}

void HttpSession::send(const char* packet, 
    size_t len, 
    const DataSocket::PACKED_SENDED_CALLBACK& callback)
{
    mSession->send(packet, len, callback);
}

void HttpSession::postShutdown() const
{
    mSession->postShutdown();
}

void HttpSession::postClose() const
{
    mSession->postDisConnect();
}

HttpSession::PTR HttpSession::Create(TCPSession::PTR session)
{
    struct make_shared_enabler : public HttpSession
    {
    public:
        make_shared_enabler(TCPSession::PTR session) : HttpSession(std::move(session))
        {}
    };

    return std::make_shared<make_shared_enabler>(std::move(session));
}

void HttpService::setup(const TCPSession::PTR& session, 
    const HttpSession::ENTER_CALLBACK& enterCallback)
{
    auto httpSession = HttpSession::Create(session);
    if (enterCallback != nullptr)
    {
        enterCallback(httpSession);
    }
    HttpService::handle(httpSession);
}

size_t HttpService::ProcessWebSocket(const char* buffer, 
    size_t len,
    const HTTPParser::PTR& httpParser,
    const HttpSession::PTR& httpSession)
{
    const char* parse_str = buffer;
    size_t leftLen = len;

    auto& cacheFrame = httpParser->getWSCacheFrame();
    auto& parseString = httpParser->getWSParseString();

    while (leftLen > 0)
    {
        parseString.clear();

        auto opcode = WebSocketFormat::WebSocketFrameType::ERROR_FRAME;
        size_t frameSize = 0;
        bool isFin = false;

        if (!WebSocketFormat::wsFrameExtractBuffer(parse_str, leftLen, parseString, opcode, frameSize, isFin))
        {
            // 如果没有解析出完整的ws frame则退出函数
            break;
        }

        // 如果当前fram的fin为false或者opcode为延续包，则将当前frame的payload添加到cache
        if (!isFin || opcode == WebSocketFormat::WebSocketFrameType::CONTINUATION_FRAME)
        {
            cacheFrame += std::move(parseString);
        }
        // 如果当前fram的fin为false，并且opcode不为延续包，则表示收到分段payload的第一个段(frame)，需要缓存当前frame的opcode
        if (!isFin && opcode != WebSocketFormat::WebSocketFrameType::CONTINUATION_FRAME)
        {
            httpParser->cacheWSFrameType(opcode);
        }

        if ( isFin)
        {
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

            const auto& wsCallback = httpSession->getWSCallback();
            if (wsCallback != nullptr)
            {
                wsCallback(httpSession, opcode, parseString);
            }
        }

        leftLen -= frameSize;
        parse_str += frameSize;
    }

    return parse_str - buffer;
}

size_t HttpService::ProcessHttp(const char* buffer,
    size_t len,
    const HTTPParser::PTR& httpParser,
    const HttpSession::PTR& httpSession)
{
    const auto retlen = httpParser->tryParse(buffer, len);
    if (!httpParser->isCompleted())
    {
        return retlen;
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

void HttpService::handle(const HttpSession::PTR& httpSession)
{
    /*TODO::keep alive and timeout close */
    auto& session = httpSession->getSession();

    session->setDisConnectCallback([httpSession](const TCPSession::PTR& session){
        const auto& tmp = httpSession->getCloseCallback();
        if (tmp != nullptr)
        {
            tmp(httpSession);
        }
    });

    auto httpParser = std::make_shared<HTTPParser>(HTTP_BOTH);
    session->setDataCallback([httpSession, httpParser](
                                const TCPSession::PTR& session, 
                                const char* buffer, size_t len){
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
