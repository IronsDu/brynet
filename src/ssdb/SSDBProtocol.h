#ifndef _SSDB_PROTOCOL_H
#define _SSDB_PROTOCOL_H

#include <string>
#include <vector>
#include <unordered_map>
#include <stdint.h>

#define DEFAULT_SSDBPROTOCOL_LEN 1024

struct buffer_s;

/*  ssdb请求结果的状态    */
class Status
{
public:
    Status();
    Status(std::string&&);
    Status(const std::string& code);
    Status(Status&&);

    Status& operator =(Status&&);

    int             not_found() const;
    int             ok() const;
    int             error() const;

    std::string     code() const;

private:
    std::string     mCode;
};

/*  ssdb协议的请求格式 */
class SSDBProtocolRequest
{
public:
    SSDBProtocolRequest();

    ~SSDBProtocolRequest();

    void            appendStr(const char* str);

    void            appendInt64(int64_t val);

    void            appendStr(const std::string& str);

    void            endl();

    void            appendBlock(const char* data, int len);

    const char*     getResult();
    int             getResultLen();

    void            init();

    template<typename Arg1, typename... Args>
    void            writev(const Arg1& arg1, const Args&... args)
    {
        this->operator<<(arg1);
        writev(args...);
    }

    void            writev()
    {
    }

private:

    SSDBProtocolRequest & operator << (const std::vector<std::string> &keys)
    {
        for (auto& v : keys)
        {
            appendStr(v);
        }
        return *this;
    }

    SSDBProtocolRequest & operator << (const std::unordered_map<std::string, std::string> &kvs)
    {
        for (auto& it : kvs)
        {
            appendStr(it.first);
            appendStr(it.second);
        }
        return *this;
    }

    SSDBProtocolRequest & operator << (const int64_t &v)
    {
        appendInt64(v);
        return *this;
    }

    SSDBProtocolRequest & operator << (const char* const &v)
    {
        appendStr(v);
        return *this;
    }

    SSDBProtocolRequest & operator << (const std::string &v)
    {
        appendStr(v);
        return *this;
    }
private:
    buffer_s*       m_request;
};

struct Bytes
{
    const char* buffer;
    int len;
};

/*  ssdb返回值的协议格式    */
class SSDBProtocolResponse
{
public:
    ~SSDBProtocolResponse();

    void                init();

    void                parse(const char* buffer, int len);
    Bytes*              getByIndex(size_t index);
    void                pushByte(const char* buffer, int len);

    size_t              getBuffersLen() const;

    Status              getStatus();

    static int          check_ssdb_packet(const char* buffer, int len);

private:
    std::vector<Bytes>   mBuffers;
};

Status read_list(SSDBProtocolResponse *response, std::vector<std::string> *ret);
Status read_int64(SSDBProtocolResponse *response, int64_t *ret);
Status read_str(SSDBProtocolResponse *response, std::string *ret);


#endif