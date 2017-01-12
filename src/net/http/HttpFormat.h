#ifndef DODO_NET_HTTPFORMAT_H_
#define DODO_NET_HTTPFORMAT_H_

#include <string>
#include <map>

namespace dodo
{
    namespace net
    {
        class HttpFormat
        {
        public:

            enum class HTTP_RESPONSE_STATUS
            {
                NONE,
                OK = 200,
            };

            enum HTTP_TYPE_PROTOCOL
            {
                HTP_NONE,
                HTP_GET,
                HTP_POST,
                HTP_PUT,
                HTP_RESPONSE,
                HTP_MAX
            };

            HttpFormat()
            {
                setStatus(HTTP_RESPONSE_STATUS::OK);
            }

            void        setStatus(HTTP_RESPONSE_STATUS status)
            {
                mStatus = status;
            }

            void        setProtocol(HTTP_TYPE_PROTOCOL protocol)
            {
                mProtocol = protocol;
                assert(mProtocol > HTP_NONE && mProtocol < HTP_MAX);
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
                mUrl = url;
            }

            void        addParameter(const char* v)
            {
                mParameter += v;
            }

            void        addParameter(const char* k, const char* v)
            {
                if (mParameter.size() > 0)
                {
                    mParameter += "&";
                }

                mParameter += k;
                mParameter += "=";
                mParameter += v;
            }

            void        addHeadValue(std::string field, std::string value)
            {
                mHeadField[field] = value;
            }

            std::string      getResult() const
            {
                std::string ret;
                if (mProtocol == HTP_GET)
                {
                    ret += "GET";
                }
                else if (mProtocol == HTP_POST)
                {
                    ret += "POST";
                }
                else if (mProtocol == HTP_PUT)
                {
                    ret += "PUT";
                }

                ret += " ";
                ret += mUrl;
                if (mProtocol == HTP_GET && !mParameter.empty())
                {
                    ret += "?";
                    ret += mParameter;
                }

                ret += " HTTP/1.1";
                if (mProtocol == HTP_RESPONSE)
                {
                    ret += " ";
                    ret += std::to_string(static_cast<int>(mStatus));
                    switch (mStatus)
                    {
                    case HTTP_RESPONSE_STATUS::OK:
                        ret += " OK";
                    default:
                        break;
                    }
                }

                ret += "\r\n";

                for (auto& v : mHeadField)
                {
                    ret += v.first;
                    ret += ": ";
                    ret += v.second;
                    ret += "\r\n";
                }

                if (mProtocol != HTP_GET && !mParameter.empty())
                {
                    ret += "Content-Length: ";
                    char temp[1024];
                    sprintf(temp, "%d", mParameter.size());
                    ret += temp;
                    ret += "\r\n";
                }

                ret += "\r\n";

                if (mProtocol != HTP_GET && !mParameter.empty())
                {
                    ret += mParameter;
                }

                return ret;
            }
        private:
            HTTP_RESPONSE_STATUS                mStatus;
            std::string                         mUrl;
            HTTP_TYPE_PROTOCOL                  mProtocol;
            std::string                         mParameter;
            std::map<std::string, std::string>  mHeadField;
        };
    }
}

#endif