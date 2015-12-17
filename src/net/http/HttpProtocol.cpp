#include "http_parser.h"

using namespace std;

#include "HttpProtocol.h"

const char* HTTPProtocol::sTmpHeadStr;
size_t HTTPProtocol::sTmpHeadLen;

HTTPProtocol::HTTPProtocol(http_parser_type parserType)
{
    mISCompleted = false;
    mParserType = parserType;
    mSettings.on_status = sStatusHandle;
    mSettings.on_body = sBodyHandle;
    mSettings.on_url = sUrlHandle;
    mSettings.on_header_value = sHeadValue;
    mSettings.on_header_field = sHeadField;
    mSettings.on_headers_complete = sHeadComplete;
    mSettings.on_message_begin = sMessageBegin;
    mSettings.on_message_complete = sMessageEnd;
    mSettings.on_chunk_header = sChunkHeader;
    mSettings.on_chunk_complete = sChunkComplete;
    mParser.data = this;
    // 初始化解析器
    http_parser_init(&mParser, mParserType);
}

bool HTTPProtocol::checkCompleted(const char* buffer, int len)
{
    const static char* RL = "\r\n";
    const static int RL_LEN = strlen(RL);

    const static char* DOUBLE_RL = "\r\n\r\n";
    const static int DOUBLE_RL_LEN = strlen(DOUBLE_RL);

    const static char* CONTENT_LENGTH_FLAG = "Content-Length: ";
    const static int CONTENT_LENGTH_FLAG_LEN = strlen(CONTENT_LENGTH_FLAG);

    const static char* CHUNKED_FLAG = "Transfer-Encoding: chunked";
    const static int CHUNKED_FLAG_LEN = strlen(CHUNKED_FLAG);

    std::string copyBuffer(buffer, len);
    copyBuffer.push_back(0);

    const char* headlineend = strstr(copyBuffer.c_str(), DOUBLE_RL);
    if (headlineend == nullptr) return false;

    const char* bodystart = headlineend + DOUBLE_RL_LEN;

    const char* contentlen_find = strstr(copyBuffer.c_str(), CONTENT_LENGTH_FLAG);
    if (contentlen_find != nullptr)
    {
        char temp[1024];
        int num = 0;
        const char* content_len_flag_start = contentlen_find + CONTENT_LENGTH_FLAG_LEN;
        const char* content_len_flag_end = strstr(content_len_flag_start, "\r\n");

        for (; content_len_flag_start < content_len_flag_end; ++content_len_flag_start)
        {
            temp[num++] = *content_len_flag_start;
        }
        temp[num++] = 0;
        if (num == 1)
        {
            return false;
        }

        const int datalen = atoi(temp);
        if ((len - (bodystart - copyBuffer.c_str())) >= datalen)
        {
            return true;
        }
    }
    else
    {
        const char* has_chunked = strstr(copyBuffer.c_str(), CHUNKED_FLAG);
        if (has_chunked != nullptr)
        {
            bool checkChunked = false;

            const char* tmp = bodystart;
            const char* len_flag = strstr(bodystart, RL);
            while (len_flag != nullptr)
            {
                string numstr(tmp, len_flag);
                int    nValude = 0;
                sscanf(numstr.c_str(), "%x", &nValude);

                /*跳过Len字段后的RL*/
                len_flag += (RL_LEN);
                tmp = len_flag;
                if (tmp >= (copyBuffer.c_str() + len))
                {
                    break;
                }

                if (nValude > 0)
                {
                    /*跳过数据*/
                    len_flag += nValude;
                    tmp = len_flag;
                    if (tmp >= (copyBuffer.c_str() + len))
                    {
                        break;
                    }

                    /*跳过其后的RL*/
                    len_flag = strstr(tmp, RL);
                    if (len_flag != nullptr)
                    {
                        len_flag += RL_LEN;
                        tmp = len_flag;
                    }
                    else
                    {
                        break;
                    }
                }

                if (nValude == 0)
                {
                    checkChunked = true;
                    break;
                }
                else
                {
                    /*指向可能存在的下一个datalen末尾的rl*/
                    len_flag = strstr(tmp, RL);
                }
            }

            if (checkChunked)
            {
                /*检查是否有拖挂数据*/
                if (*tmp == '\r')
                {
                    const char* finish = strstr(tmp, RL);
                    return finish != nullptr;
                }
                else
                {
                    const char* finish = strstr(tmp, DOUBLE_RL);
                    return finish != nullptr;
                }
            }
        }
        else
        {
            return true;
        }
    }

    return false;
}

int HTTPProtocol::appendAndParse(const char* buffer, int len)
{
    if (!mISCompleted && checkCompleted(buffer, len))
    {
        mISCompleted = true;
        int nparsed = http_parser_execute(&mParser, &mSettings, buffer, len);
        http_parser_init(&mParser, mParserType);
        return len;
    }
    else
    {
        return 0;
    }
}

const std::string& HTTPProtocol::getPath() const
{
    return mPath;
}

const std::string& HTTPProtocol::getQuery() const
{
    return mQuery;
}

bool HTTPProtocol::isCompleted() const
{
    return mISCompleted;
}

std::string HTTPProtocol::getValue(const std::string& key) const
{
    auto it = mHeadValues.find(key);
    if (it != mHeadValues.end())
    {
        return (*it).second;
    }
    else
    {
        return "";
    }
}

int HTTPProtocol::sChunkHeader(http_parser* hp)
{
    return 0;
}
int HTTPProtocol::sChunkComplete(http_parser* hp)
{
    return 0;
}
int HTTPProtocol::sMessageBegin(http_parser* hp)
{
    return 0;
}
int HTTPProtocol::sMessageEnd(http_parser* hp)
{
    return 0;
}
int HTTPProtocol::sHeadComplete(http_parser* hp)
{
    return 0;
}

int HTTPProtocol::sUrlHandle(http_parser* hp, const char *url, size_t length)
{
    struct http_parser_url u;
    HTTPProtocol* httpProtocol = (HTTPProtocol*)hp->data;

    int result = http_parser_parse_url(url, length, 0, &u);
    if (result) {
        return -1;
    }
    else {
        if ((u.field_set & (1 << UF_PATH))) {
            httpProtocol->mPath = std::string(url + u.field_data[UF_PATH].off, u.field_data[UF_PATH].len);
        }
        else {
            fprintf(stderr, "\n\n*** failed to parse PATH in URL %s ***\n\n", url);
            return -1;
        }

        if ((u.field_set & (1 << UF_QUERY))) {
            httpProtocol->mQuery = std::string(url + u.field_data[UF_QUERY].off, u.field_data[UF_QUERY].len);
        }
    }

    return 0;
}

int HTTPProtocol::sHeadValue(http_parser* hp, const char *at, size_t length)
{
    HTTPProtocol* httpProtocol = (HTTPProtocol*)hp->data;
    httpProtocol->mHeadValues[string(sTmpHeadStr, sTmpHeadLen)] = string(at, length);
    printf("Header field: %.*s\n", (int)length, at);
    return 0;
}

int HTTPProtocol::sHeadField(http_parser* hp, const char *at, size_t length)
{
    sTmpHeadStr = at;
    sTmpHeadLen = length;
    printf("Header value: %.*s\n", (int)length, at);
    return 0;
}

int HTTPProtocol::sStatusHandle(http_parser* hp, const char *at, size_t length)
{
    printf("Body: %.*s\n", (int)length, at);
    return 0;
}
int HTTPProtocol::sBodyHandle(http_parser* hp, const char *at, size_t length)
{
    printf("Body: %.*s\n", (int)length, at);
    return 0;
}
