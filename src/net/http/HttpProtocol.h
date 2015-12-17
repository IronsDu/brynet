#ifndef _HTTP_PROTOCOL_H
#define _HTTP_PROTOCOL_H

#include <string>
#include <map>

#include "http_parser.h"

class HTTPProtocol
{
public:
    HTTPProtocol(http_parser_type parserType);

    bool                                    checkCompleted(const char* buffer, int len);
    int                                     appendAndParse(const char* buffer, int len);

    const std::string&                      getPath() const;

    const std::string&                      getQuery() const;

    bool                                    isCompleted() const;

    std::string                             getValue(const std::string& key) const;

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
    bool                                    mISCompleted;
    http_parser_type                        mParserType;
    std::string                             mPath;
    std::string                             mQuery;
    http_parser                             mParser;
    http_parser_settings                    mSettings;
    std::map<std::string, std::string>      mHeadValues;

    static const char*                      sTmpHeadStr;
    static size_t                           sTmpHeadLen;
};

#endif