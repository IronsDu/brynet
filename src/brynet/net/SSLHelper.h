#ifndef BRYNET_NET_SSL_HELPER_H_
#define BRYNET_NET_SSL_HELPER_H_

#include <string>
#include <memory>

#include <brynet/utils/NonCopyable.h>
#include <brynet/net/Noexcept.h>

#ifdef USE_OPENSSL

#ifdef  __cplusplus
extern "C" {
#endif
#include <openssl/ssl.h>
#include <openssl/err.h>
#ifdef  __cplusplus
}
#endif

#endif

namespace brynet
{
    namespace net
    {
        class SSLHelper : public NonCopyable, public std::enable_shared_from_this<SSLHelper>
        {
        public:
            typedef std::shared_ptr<SSLHelper>   PTR;

#ifdef USE_OPENSSL
            bool                                initSSL(const std::string& certificate, 
                                                        const std::string& privatekey);
            void                                destroySSL();
            SSL_CTX*                            getOpenSSLCTX();
#endif
            static  PTR                         Create();

        private:
            SSLHelper() BRYNET_NOEXCEPT;
            virtual ~SSLHelper() BRYNET_NOEXCEPT;

        private:
#ifdef USE_OPENSSL
            SSL_CTX*                            mOpenSSLCTX;
#endif
        };
    }
}

#endif
