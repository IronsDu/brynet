#include <unordered_map>
#include <mutex>
#include <thread>

#include <brynet/net/Platform.h>
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

static void cryptoSetThreadIDCallback(CRYPTO_THREADID* id)
{
#ifdef PLATFORM_WINDOWS
	CRYPTO_THREADID_set_numeric(id, static_cast<unsigned long>(GetCurrentThreadId()));
#else
	CRYPTO_THREADID_set_numeric(id, static_cast<unsigned long>(pthread_self()));
#endif
}

static std::unordered_map<int, std::shared_ptr<std::mutex>> cryptoLocks;
static void cryptoLockingCallback(int mode, 
    int type,
    const char *file, int line)
{
    if (mode & CRYPTO_LOCK)
    {
		cryptoLocks[type]->lock();
    }
    else if(mode & CRYPTO_UNLOCK)
    {
		cryptoLocks[type]->unlock();
    }
}

static std::once_flag initCryptoThreadSafeSupportOnceFlag;
static void InitCryptoThreadSafeSupport()
{
	for (int i = 0; i < CRYPTO_num_locks(); i++)
	{
		cryptoLocks[i] = std::make_shared<std::mutex>();
	}

	CRYPTO_THREADID_set_callback(cryptoSetThreadIDCallback);
	CRYPTO_set_locking_callback(cryptoLockingCallback);
}

bool SSLHelper::initSSL(const std::string& certificate, const std::string& privatekey)
{
	std::call_once(initCryptoThreadSafeSupportOnceFlag, InitCryptoThreadSafeSupport);

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