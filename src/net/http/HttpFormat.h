#ifndef _HTTPREQUEST_H
#define _HTTPFORMAT_H

#include <string>
using namespace std;

class HttpFormat
{
public:
    enum HTTPREQUEST_PROTOCOL
    {
        HRP_NONE,
        HRP_GET,
        HRP_POST,
        HRP_PUT,
        HRP_RESPONSE,
        HRP_MAX
    };

    void        setProtocol(HTTPREQUEST_PROTOCOL protocol)
    {
        m_eProtocol = protocol;
        assert(m_eProtocol > HRP_NONE && m_eProtocol < HRP_MAX);
    }

    void        setHost(const char* host)
    {
        addHeadValue("Host", host);
    }

    void        setCookie(const char* v)
    {
        addHeadValue("Cookie", v);
    }

    void        setContentType(const char* v)
    {
        addHeadValue("Content-Type", v);
    }

    void        setRequestUrl(const char* url)
    {
        m_url = url;
    }

    void        addParameter(const char* v)
    {
        m_parameter += v;
    }

    void        addParameter(const char* k, const char* v)
    {
        if(m_parameter.size() > 0)
        {
            m_parameter += "&";
        }

        m_parameter += k;
        m_parameter += "=";
        m_parameter += v;
    }

    void        addHeadValue(std::string field, std::string value)
    {
        mHeadField[field] = value;
    }

    string      getResult()
    {
        string ret;
        if(m_eProtocol == HRP_GET)
        {
            ret += "GET";
        }
        else if (m_eProtocol == HRP_POST)
        {
            ret += "POST";
        }
        else if (m_eProtocol == HRP_PUT)
        {
            ret += "PUT";
        }

        ret += " ";
        ret += m_url;
        if (m_eProtocol == HRP_GET && !m_parameter.empty())
        {
            ret += "?";
            ret += m_parameter;
        }

        ret += " HTTP/1.1";
        ret += "\r\n";

        for (auto& v : mHeadField)
        {
            ret += v.first;
            ret += ": ";
            ret += v.second;
            ret += "\r\n";
        }

        if (m_eProtocol != HRP_GET && !m_parameter.empty())
        {
            ret += "Content-Length: ";
            char temp[1024];
            sprintf(temp, "%d", m_parameter.size());
            ret += temp;
            ret += "\r\n";
        }

        ret += "\r\n";

        if (m_eProtocol != HRP_GET && !m_parameter.empty())
        {
            ret += m_parameter;
        }

        return ret;
    }
private:

    string                  m_url;
    HTTPREQUEST_PROTOCOL    m_eProtocol;
    string                  m_parameter;
    std::map<string, string>    mHeadField;
};

#endif