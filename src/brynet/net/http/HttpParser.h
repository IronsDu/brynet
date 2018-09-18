#ifndef BRYNET_NET_HTTPPARSER_H_
#define BRYNET_NET_HTTPPARSER_H_

#include <string>
#include <map>
#include <memory>

#include <brynet/net/http/http_parser.h>
#include <brynet/net/http/WebSocketFormat.h>

namespace brynet
{
    namespace net
    {
        class HttpService;

        class HTTPParser
        {
        public:
            typedef std::shared_ptr<HTTPParser> PTR;

            explicit HTTPParser(http_parser_type parserType);
            bool                                    isWebSocket() const;
            bool                                    isKeepAlive() const;

            const std::string&                      getPath() const;
            const std::string&                      getQuery() const;
            const std::string&                      getStatus() const;
            int                                     getStatusCode() const;

            bool                                    hasEntry(const std::string& key, const std::string& value) const;
            bool                                    hasKey(const std::string& key) const;
            const std::string&                      getValue(const std::string& key) const;
            const std::string&                      getBody() const;

            std::string&                            getWSCacheFrame();
            std::string&                            getWSParseString();
            WebSocketFormat::WebSocketFrameType     getWSFrameType() const;
            void                                    cacheWSFrameType(WebSocketFormat::WebSocketFrameType frameType);

        private:
            void                                    clearParse();
            size_t                                  tryParse(const char* buffer, size_t len);
            bool                                    isCompleted() const;

        private:
            static int                              sChunkHeader(http_parser* hp);
            static int                              sChunkComplete(http_parser* hp);
            static int                              sMessageBegin(http_parser* hp);
            static int                              sMessageEnd(http_parser* hp);
            static int                              sHeadComplete(http_parser* hp);
            static int                              sUrlHandle(http_parser* hp, const char *url, size_t length);
            static int                              sHeadValue(http_parser* hp, const char *at, size_t length);
            static int                              sHeadField(http_parser* hp, const char *at, size_t length);
            static int                              sStatusHandle(http_parser* hp, const char *at, size_t length);
            static int                              sBodyHandle(http_parser* hp, const char *at, size_t length);

        private:
            http_parser_type                        mParserType;
            http_parser                             mParser;
            http_parser_settings                    mSettings;

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
    }
}

#endif