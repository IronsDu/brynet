#include <brynet/net/Noexcept.h>
#include <brynet/net/SSLHelper.h>
                                              
using namespace brynet::net;

SSLHelper::PTR SSLHelper::Create()
{
    struct make_shared_enabler : public SSLHelper {};
    return std::make_shared<make_shared_enabler>();
}

SSLHelper::SSLHelper() BRYNET_NOEXCEPT
{
#ifdef USE_OPENSSL
    mOpenSSLCTX = nullptr;
#endif
}

SSLHelper::~SSLHelper() BRYNET_NOEXCEPT
{
#ifdef USE_OPENSSL
    destroySSL();
#endif
}

#ifdef USE_OPENSSL
SSL_CTX* SSLHelper::getOpenSSLCTX()
{
    return mOpenSSLCTX;
}

bool SSLHelper::initSSL(const std::string& certificate, const std::string& privatekey)
{
    if (mOpenSSLCTX != nullptr)
    {
        return false;
    }
    if (certificate.empty() || privatekey.empty())
    {
        return false;
    }

    mOpenSSLCTX = SSL_CTX_new(SSLv23_method());
    SSL_CTX_set_client_CA_list(mOpenSSLCTX, SSL_load_client_CA_file(certificate.c_str()));
    SSL_CTX_set_verify_depth(mOpenSSLCTX, 10);

    if (SSL_CTX_use_certificate_file(mOpenSSLCTX, certificate.c_str(), SSL_FILETYPE_PEM) <= 0)
    {
        SSL_CTX_free(mOpenSSLCTX);
        mOpenSSLCTX = nullptr;
        return false;
    }

    if (SSL_CTX_use_PrivateKey_file(mOpenSSLCTX, privatekey.c_str(), SSL_FILETYPE_PEM) <= 0)
    {
        SSL_CTX_free(mOpenSSLCTX);
        mOpenSSLCTX = nullptr;
        return false;
    }

    if (!SSL_CTX_check_private_key(mOpenSSLCTX))
    {
        SSL_CTX_free(mOpenSSLCTX);
        mOpenSSLCTX = nullptr;
        return false;
    }

    return true;
}

void SSLHelper::destroySSL()
{
    if(mOpenSSLCTX != nullptr)
    {
        SSL_CTX_free(mOpenSSLCTX);
        mOpenSSLCTX = nullptr;
    }
}
#endif