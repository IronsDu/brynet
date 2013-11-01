#ifndef _HTTPREQUEST_H
#define _HTTPREQUEST_H

#include <string>
using namespace std;

enum HTTPREQUEST_PROTOCOL
{
    HRP_GET,
    HRP_POST,
};

class HttpRequest
{
public:
    void        setProtocol(HTTPREQUEST_PROTOCOL protocol)
    {
        m_eProtocol = protocol;
        assert(m_eProtocol >= HRP_GET && m_eProtocol <= HRP_POST);
    }

    void        setHost(const char* host)
    {
        m_host = host;
    }

    void        setRequestUrl(const char* url)
    {
        m_url = url;
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

    string      getResult()
    {
        string ret;
        if(m_eProtocol == HRP_GET)
        {
            ret += "GET";
        }
        else
        {
            ret += "POST";
        }

        ret += " ";
        ret += m_url;
        if(m_eProtocol == HRP_GET)
        {
            ret += "?";
            ret += m_parameter;
        }

        ret += " HTTP/1.1";
        ret += "\r\n";

        ret += "Host: ";
        ret += m_host;
        ret += "\r\n";

        if(m_eProtocol == HRP_POST)
        {
            ret += "Content-Length: ";
            char temp[1024];
            sprintf(temp, "%d", m_parameter.size());
            ret += temp;
            ret += "\r\n";

            ret += "Content-Type: application/x-www-form-urlencoded\r\n";
        }

        ret += "\r\n";

        if(m_eProtocol == HRP_POST)
        {
            ret += m_parameter;
            ret += "\r\n";
        }

        return ret;
    }
private:

    string                  m_host;
    string                  m_url;
    HTTPREQUEST_PROTOCOL    m_eProtocol;
    string                  m_parameter;
};

#endif