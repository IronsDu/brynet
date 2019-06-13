#include <cassert>
#include <cstring>
#include <brynet/net/http/http_parser.h>

#include <brynet/net/http/HttpParser.h>

namespace brynet { namespace net { namespace http {

    HTTPParser::HTTPParser(http_parser_type parserType) 
        :
        mParserType(parserType)
    {
        mLastWasValue = true;

        mIsWebSocket = false;
        mIsKeepAlive = false;
        mISCompleted = false;
        mSettings.on_status = sStatusHandle;
        mSettings.on_body = sBodyHandle;
        mSettings.on_url = sUrlHandle;
        mSettings.on_header_field = sHeadField;
        mSettings.on_header_value = sHeadValue;
        mSettings.on_headers_complete = sHeadComplete;
        mSettings.on_message_begin = sMessageBegin;
        mSettings.on_message_complete = sMessageEnd;
        mSettings.on_chunk_header = sChunkHeader;
        mSettings.on_chunk_complete = sChunkComplete;
        mParser.data = this;

        http_parser_init(&mParser, mParserType);
    }

    void HTTPParser::clearParse()
    {
        mISCompleted = false;
        mHeadValues.clear();
        mPath.clear();
        mQuery.clear();
    }

    bool HTTPParser::isWebSocket() const
    {
        return mIsWebSocket;
    }

    bool HTTPParser::isKeepAlive() const
    {
        return mIsKeepAlive;
    }

    size_t HTTPParser::tryParse(const char* buffer, size_t len)
    {
        size_t nparsed = http_parser_execute(&mParser, &mSettings, buffer, len);
        if (mISCompleted)
        {
            mIsWebSocket = mParser.upgrade;
            mIsKeepAlive = !hasEntry("Connection", "close");
            http_parser_init(&mParser, mParserType);
        }

        return nparsed;
    }

    const std::string& HTTPParser::getPath() const
    {
        return mPath;
    }

    const std::string& HTTPParser::getQuery() const
    {
        return mQuery;
    }

    const std::string& HTTPParser::getStatus() const
    {
        return mStatus;
    }

    int HTTPParser::getStatusCode() const
    {
        return mStatusCode;
    }

    bool HTTPParser::isCompleted() const
    {
        return mISCompleted;
    }

    bool HTTPParser::hasEntry(const std::string& key, const std::string& value) const
    {
        auto it = mHeadValues.find(key);
        return it != mHeadValues.end() && value == it->second;
    }

    bool HTTPParser::hasKey(const std::string& key) const
    {
        return mHeadValues.find(key) != mHeadValues.end();
    }

    const std::string& HTTPParser::getValue(const std::string& key) const
    {
        const static std::string emptystr("");

        auto it = mHeadValues.find(key);
        if (it != mHeadValues.end())
        {
            return (*it).second;
        }
        else
        {
            return emptystr;
        }
    }

    const std::string& HTTPParser::getBody() const
    {
        return mBody;
    }

    std::string& HTTPParser::getWSCacheFrame()
    {
        return mWSCacheFrame;
    }

    std::string& HTTPParser::getWSParseString()
    {
        return mWSParsePayload;
    }

    WebSocketFormat::WebSocketFrameType HTTPParser::getWSFrameType() const
    {
        return mWSFrameType;
    }

    void HTTPParser::cacheWSFrameType(WebSocketFormat::WebSocketFrameType frameType)
    {
        mWSFrameType = frameType;
    }

    int HTTPParser::sChunkHeader(http_parser* hp)
    {
        (void)hp;
        return 0;
    }
    int HTTPParser::sChunkComplete(http_parser* hp)
    {
        (void)hp;
        return 0;
    }
    int HTTPParser::sMessageBegin(http_parser* hp)
    {
        HTTPParser* httpParser = (HTTPParser*)hp->data;
        httpParser->mLastWasValue = true;
        httpParser->mCurrentField.clear();
        httpParser->mCurrentValue.clear();

        return 0;
    }

    int HTTPParser::sMessageEnd(http_parser* hp)
    {
        HTTPParser* httpParser = (HTTPParser*)hp->data;
        httpParser->mISCompleted = true;
        return 0;
    }

    int HTTPParser::sHeadComplete(http_parser* hp)
    {
        HTTPParser* httpParser = (HTTPParser*)hp->data;
        if (!httpParser->mCurrentField.empty())
        {
            httpParser->mHeadValues[httpParser->mCurrentField] = httpParser->mCurrentValue;
        }

        if (httpParser->mUrl.empty())
        {
            return 0;
        }

        struct http_parser_url u;

        const int result = http_parser_parse_url(httpParser->mUrl.data(), httpParser->mUrl.size(), 0, &u);
        if (result != 0)
        {
            return -1;
        }

        if (!(u.field_set & (1 << UF_PATH)))
        {
            fprintf(stderr, "\n\n*** failed to parse PATH in URL %s ***\n\n", httpParser->mUrl.c_str());
            return -1;
        }

        httpParser->mPath = std::string(httpParser->mUrl.data() + u.field_data[UF_PATH].off, u.field_data[UF_PATH].len);
        if (u.field_set & (1 << UF_QUERY))
        {
            httpParser->mQuery = std::string(httpParser->mUrl.data() + u.field_data[UF_QUERY].off, u.field_data[UF_QUERY].len);
        }

        return 0;
    }

    int HTTPParser::sUrlHandle(http_parser* hp, const char *url, size_t length)
    {
        HTTPParser* httpParser = (HTTPParser*)hp->data;
        httpParser->mUrl.append(url, length);

        return 0;
    }

    int HTTPParser::sHeadValue(http_parser* hp, const char *at, size_t length)
    {
        HTTPParser* httpParser = (HTTPParser*)hp->data;
        httpParser->mCurrentValue.append(at, length);
        httpParser->mLastWasValue = true;
        return 0;
    }

    int HTTPParser::sHeadField(http_parser* hp, const char *at, size_t length)
    {
        HTTPParser* httpParser = (HTTPParser*)hp->data;
        if (httpParser->mLastWasValue)
        {
            if (!httpParser->mCurrentField.empty())
            {
                sHeadComplete(hp);
            }
            httpParser->mCurrentField.clear();
            httpParser->mCurrentValue.clear();
        }

        httpParser->mCurrentField.append(at, length);
        httpParser->mLastWasValue = false;

        return 0;
    }

    int HTTPParser::sStatusHandle(http_parser* hp, const char *at, size_t length)
    {
        HTTPParser* httpParser = (HTTPParser*)hp->data;
        httpParser->mStatus.append(at, length);
        httpParser->mStatusCode = hp->status_code;
        return 0;
    }

    int HTTPParser::sBodyHandle(http_parser* hp, const char *at, size_t length)
    {
        HTTPParser* httpParser = (HTTPParser*)hp->data;
        httpParser->mBody.append(at, length);
        return 0;
    }

} } }