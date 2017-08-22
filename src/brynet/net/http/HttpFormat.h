#ifndef BRYNET_NET_HTTPFORMAT_H_
#define BRYNET_NET_HTTPFORMAT_H_

#include <string>
#include <array>
#include <unordered_map>
#include <cassert>

namespace brynet
{
    namespace net
    {
        class HttpQueryParameter
        {
        public:
            void        add(const std::string& v)
            {
                mParameter += v;
            }

            void        add(const std::string& k, const std::string& v)
            {
                if (mParameter.size() > 0)
                {
                    mParameter += "&";
                }

                mParameter += k;
                mParameter += "=";
                mParameter += v;
            }

            std::string getResult() const
            {
                return mParameter;
            }

        private:
            std::string mParameter;
        };

        class HttpRequest
        {
        public:

            enum class HTTP_METHOD
            {
                HTTP_METHOD_HEAD,
                HTTP_METHOD_GET,
                HTTP_METHOD_POST,
                HTTP_METHOD_PUT,
                HTTP_METHOD_DELETE,
                HTTP_METHOD_MAX
            };

            HttpRequest()
            {
                setMethod(HTTP_METHOD::HTTP_METHOD_GET);
            }

            void        setMethod(HTTP_METHOD protocol)
            {
                mMethod = protocol;
                assert(mMethod > HTTP_METHOD::HTTP_METHOD_HEAD && mMethod < HTTP_METHOD::HTTP_METHOD_MAX);
            }

            void        setHost(const std::string& host)
            {
                addHeadValue("Host", host);
            }

            void        setUrl(const std::string& url)
            {
                mUrl = url;
            }

            void        setCookie(const std::string& v)
            {
                addHeadValue("Cookie", v);
            }

            void        setContentType(const std::string& v)
            {
                addHeadValue("Content-Type", v);
            }

            void        setQuery(const std::string& query)
            {
                mQuery = query;
            }

            void        setBody(const std::string& body)
            {
                mBody = body;
                addHeadValue("Content-Length", std::to_string(body.size()));
            }

            void        addHeadValue(const std::string& field, const std::string& value)
            {
                mHeadField[field] = value;
            }

            std::string      getResult() const
            {
                const static std::array<std::string, static_cast<size_t>(HTTP_METHOD::HTTP_METHOD_MAX)> HttpMethodString =
                        {"HEAD", "GET", "POST", "PUT", "DELETE"};

                std::string ret;
                if (mMethod >= HTTP_METHOD::HTTP_METHOD_HEAD && mMethod < HTTP_METHOD::HTTP_METHOD_MAX)
                {
                    ret += HttpMethodString[static_cast<size_t>(mMethod)];
                }

                ret += " ";
                ret += mUrl;
                if (mQuery.empty())
                {
                    ret += "?";
                    ret += mQuery;
                }

                ret += " HTTP/1.1\r\n";

                for (auto& v : mHeadField)
                {
                    ret += v.first;
                    ret += ": ";
                    ret += v.second;
                    ret += "\r\n";
                }

                ret += "\r\n";

                if (!mBody.empty())
                {
                    ret += mBody;
                }

                return ret;
            }

        private:
            std::string                         mUrl;
            std::string                         mQuery;
            std::string                         mBody;
            HTTP_METHOD                         mMethod;
            std::unordered_map<std::string, std::string>  mHeadField;
        };

        class HttpResponse
        {
        public:

            enum class HTTP_RESPONSE_STATUS
            {
                NONE,
                OK = 200,
            };

            HttpResponse()
            {
                setStatus(HTTP_RESPONSE_STATUS::OK);
            }

            void        setStatus(HTTP_RESPONSE_STATUS status)
            {
                mStatus = status;
            }

            void        setContentType(const std::string& v)
            {
                addHeadValue("Content-Type", v);
            }

            void        addHeadValue(const std::string& field, const std::string& value)
            {
                mHeadField[field] = value;
            }

            void        setBody(const std::string& body)
            {
                mBody = body;
                addHeadValue("Content-Length", std::to_string(body.size()));
            }

            std::string      getResult() const
            {
                std::string ret = "HTTP/1.1 ";

                ret += std::to_string(static_cast<int>(mStatus));
                switch (mStatus)
                {
                case HTTP_RESPONSE_STATUS::OK:
                    ret += " OK";
                default:
                    ret += "UNKNOWN";
                    break;
                }

                ret += "\r\n";

                for (auto& v : mHeadField)
                {
                    ret += v.first;
                    ret += ": ";
                    ret += v.second;
                    ret += "\r\n";
                }

                ret += "\r\n";

                if (!mBody.empty())
                {
                    ret += mBody;
                }

                return ret;
            }

        private:
            HTTP_RESPONSE_STATUS                mStatus;
            std::unordered_map<std::string, std::string>  mHeadField;
            std::string                         mBody;
        };
    }
}

#endif