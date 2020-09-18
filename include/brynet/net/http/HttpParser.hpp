#pragma once

#include <string>
#include <map>
#include <memory>
#include <cassert>
#include <cstring>

#include "http_parser.h"
#include <brynet/net/http/WebSocketFormat.hpp>

namespace brynet { namespace net { namespace http {

    class HttpService;

    class HTTPParser
    {
    public:
        using Ptr = std::shared_ptr<HTTPParser>;

        explicit HTTPParser(http_parser_type parserType)
            :
            mParserType(parserType)
        {
            mLastWasValue = true;

            mIsWebSocket = false;
            mIsKeepAlive = false;
            mISCompleted = false;
            mStatusCode = 0;
            mWSFrameType = WebSocketFormat::WebSocketFrameType::ERROR_FRAME;
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

        virtual ~HTTPParser() = default;

        bool    isWebSocket() const
        {
            return mIsWebSocket;
        }

        bool    isKeepAlive() const
        {
            return mIsKeepAlive;
        }

        int     method() const
        {
            // mMethod's value defined in http_method, such as  HTTP_GET、HTTP_POST.
            // if mMethod is -1, it's invalid.
            return mMethod;
        }

        const std::string& getPath() const
        {
            return mPath;
        }

        const std::string& getQuery() const
        {
            return mQuery;
        }

        const std::string& getStatus() const
        {
            return mStatus;
        }

        int         getStatusCode() const
        {
            return mStatusCode;
        }

        bool        hasEntry(const std::string& key, 
            const std::string& value) const
        {
            const auto it = mHeadValues.find(key);
            return it != mHeadValues.end() && value == it->second;
        }

        bool        hasKey(const std::string& key) const
        {
            return mHeadValues.find(key) != mHeadValues.end();
        }

        const std::string& getValue(const std::string& key) const
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

        const std::string& getBody() const
        {
            return mBody;
        }

        std::string& getWSCacheFrame()
        {
            return mWSCacheFrame;
        }

        std::string& getWSParseString()
        {
            return mWSParsePayload;
        }

        WebSocketFormat::WebSocketFrameType     getWSFrameType() const
        {
            return mWSFrameType;
        }

        void    cacheWSFrameType(WebSocketFormat::WebSocketFrameType frameType)
        {
            mWSFrameType = frameType;
        }

    private:
        void    clearParse()
        {
            mMethod = -1;
            mISCompleted = false;
            mLastWasValue = true;
            mUrl.clear();
            mQuery.clear();
            mBody.clear();
            mStatus.clear();
            mCurrentField.clear();
            mCurrentValue.clear();
            mHeadValues.clear();
            mPath.clear();
        }

        size_t  tryParse(const char* buffer, size_t len)
        {
            const size_t nparsed = http_parser_execute(&mParser, &mSettings, buffer, len);
            if (mISCompleted)
            {
                mIsWebSocket = mParser.upgrade;
                mIsKeepAlive = hasEntry("Connection", "Keep-Alive");
                mMethod = mParser.method;
                http_parser_init(&mParser, mParserType);
            }

            return nparsed;
        }

        bool    isCompleted() const
        {
            return mISCompleted;
        }

    private:
        static int  sChunkHeader(http_parser* hp)
        {
            (void)hp;
            return 0;
        }

        static int  sChunkComplete(http_parser* hp)
        {
            (void)hp;
            return 0;
        }

        static int  sMessageBegin(http_parser* hp)
        {
            HTTPParser* httpParser = (HTTPParser*)hp->data;
            httpParser->clearParse();

            return 0;
        }

        static int  sMessageEnd(http_parser* hp)
        {
            HTTPParser* httpParser = (HTTPParser*)hp->data;
            httpParser->mISCompleted = true;
            return 0;
        }

        static int  sHeadComplete(http_parser* hp)
        {
            HTTPParser* httpParser = (HTTPParser*)hp->data;

            if (httpParser->mUrl.empty())
            {
                return 0;
            }

            struct http_parser_url u;

            const int result = http_parser_parse_url(httpParser->mUrl.data(), 
                httpParser->mUrl.size(), 
                0, 
                &u);
            if (result != 0)
            {
                return -1;
            }

            if (!(u.field_set & (1 << UF_PATH)))
            {
                fprintf(stderr, 
                    "\n\n*** failed to parse PATH in URL %s ***\n\n", 
                    httpParser->mUrl.c_str());
                return -1;
            }

            httpParser->mPath = std::string(
                httpParser->mUrl.data() + u.field_data[UF_PATH].off, 
                u.field_data[UF_PATH].len);
            if (u.field_set & (1 << UF_QUERY))
            {
                httpParser->mQuery = std::string(
                    httpParser->mUrl.data() + u.field_data[UF_QUERY].off, 
                    u.field_data[UF_QUERY].len);
            }

            return 0;
        }

        static int  sUrlHandle(http_parser* hp, const char* url, size_t length)
        {
            HTTPParser* httpParser = (HTTPParser*)hp->data;
            httpParser->mUrl.append(url, length);

            return 0;
        }

        static int  sHeadValue(http_parser* hp, const char* at, size_t length)
        {
            HTTPParser* httpParser = (HTTPParser*)hp->data;
            auto& value = httpParser->mHeadValues[httpParser->mCurrentField];
            value.append(at, length);
            httpParser->mLastWasValue = true;
            return 0;
        }

        static int  sHeadField(http_parser* hp, const char* at, size_t length)
        {
            HTTPParser* httpParser = (HTTPParser*)hp->data;
            if (httpParser->mLastWasValue)
            {
                httpParser->mCurrentField.clear();
            }
            httpParser->mCurrentField.append(at, length);
            httpParser->mLastWasValue = false;

            return 0;
        }

        static int  sStatusHandle(http_parser* hp, const char* at, size_t length)
        {
            HTTPParser* httpParser = (HTTPParser*)hp->data;
            httpParser->mStatus.append(at, length);
            httpParser->mStatusCode = hp->status_code;
            return 0;
        }

        static int  sBodyHandle(http_parser* hp, const char* at, size_t length)
        {
            HTTPParser* httpParser = (HTTPParser*)hp->data;
            httpParser->mBody.append(at, length);
            return 0;
        }

    private:
        const http_parser_type                  mParserType;
        http_parser                             mParser;
        http_parser_settings                    mSettings;

        int                                     mMethod = -1;
        bool                                    mIsWebSocket;
        bool                                    mIsKeepAlive;
        bool                                    mISCompleted;

        bool                                    mLastWasValue;
        std::string                             mCurrentField;
        std::string                             mCurrentValue;

        std::string                             mPath;
        std::string                             mQuery;
        std::string                             mStatus;
        std::map<std::string, std::string>      mHeadValues;
        int                                     mStatusCode;

        std::string                             mUrl;
        std::string                             mBody;

        std::string                             mWSCacheFrame;
        std::string                             mWSParsePayload;
        WebSocketFormat::WebSocketFrameType     mWSFrameType;

    private:
        friend class HttpService;
    };

} } }