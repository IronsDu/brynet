#ifndef _HTTP_PROTOCOL_H
#define _HTTP_PROTOCOL_H

#include <string>
#include <map>

#include "http_parser.h"

class HttpServer;

class HTTPParser
{
public:
    HTTPParser(http_parser_type parserType);
    bool                                    isWebSocket() const;
    bool                                    isKeepAlive() const;

    const std::string&                      getPath() const;
    const std::string&                      getQuery() const;

    const std::string&                      getValue(const std::string& key) const;
    const std::string&                      getBody() const;

    std::string&                            getWSCacheFrame();

private:
    void                                    clearParse();
    bool                                    checkCompleted(const char* buffer, size_t len);
    /*直接尝试解析http协议,如果完整则返回报文长度，否则返回0*/
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

    std::string                             mPath;
    std::string                             mQuery;
    std::map<std::string, std::string>      mHeadValues;
    std::string                             mStatus;
    std::string                             mBody;

    std::string                             mWSCacheFrame;

private:
    const char*                             mTmpHeadStr;
    size_t                                  mTmpHeadLen;

    friend int                              sHeadValue(http_parser* hp, const char *at, size_t length);
    friend class HttpServer;
};

#endif