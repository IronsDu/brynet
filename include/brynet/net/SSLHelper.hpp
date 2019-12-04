#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <thread>

#include <brynet/base/NonCopyable.hpp>
#include <brynet/base/Platform.hpp>
#include <brynet/base/Noexcept.hpp>

#ifdef BRYNET_USE_OPENSSL

#ifdef  __cplusplus
extern "C" {
#endif
#include <openssl/ssl.h>
#include <openssl/err.h>
#ifdef  __cplusplus
}
#endif

#endif

namespace brynet { namespace net {

#ifdef BRYNET_USE_OPENSSL

#ifndef CRYPTO_THREADID_set_callback
    static void cryptoSetThreadIDCallback(CRYPTO_THREADID* id)
    {
#ifdef BRYNET_PLATFORM_WINDOWS
        CRYPTO_THREADID_set_numeric(id, 
            static_cast<unsigned long>(GetCurrentThreadId()));
#elif defined BRYNET_PLATFORM_LINUX || defined BRYNET_PLATFORM_DARWIN
        CRYPTO_THREADID_set_numeric(id, 
            static_cast<unsigned long>(pthread_self()));
#endif
    }
#endif

#ifndef CRYPTO_set_locking_callback
    static std::unordered_map<int, std::shared_ptr<std::mutex>> cryptoLocks;
    static void cryptoLockingCallback(int mode,
        int type,
        const char* file, int line)
    {
        (void)file;
        (void)line;
        if (mode & CRYPTO_LOCK)
        {
            cryptoLocks[type]->lock();
        }
        else if (mode & CRYPTO_UNLOCK)
        {
            cryptoLocks[type]->unlock();
        }
    }
#endif

    static std::once_flag initCryptoThreadSafeSupportOnceFlag;
    static void InitCryptoThreadSafeSupport()
    {
#ifndef CRYPTO_THREADID_set_callback
        CRYPTO_THREADID_set_callback(cryptoSetThreadIDCallback);
#endif

#ifndef CRYPTO_set_locking_callback
        for (int i = 0; i < CRYPTO_num_locks(); i++)
        {
            cryptoLocks[i] = std::make_shared<std::mutex>();
        }
        CRYPTO_set_locking_callback(cryptoLockingCallback);
#endif
    }
#endif

    class SSLHelper :   public brynet::base::NonCopyable, 
                        public std::enable_shared_from_this<SSLHelper>
    {
    public:
        using Ptr = std::shared_ptr<SSLHelper>;

#ifdef BRYNET_USE_OPENSSL
        bool        initSSL(const std::string& certificate,
            const std::string& privatekey)
        {
            std::call_once(initCryptoThreadSafeSupportOnceFlag, 
                InitCryptoThreadSafeSupport);

            if (mOpenSSLCTX != nullptr)
            {
                return false;
            }
            if (certificate.empty() || privatekey.empty())
            {
                return false;
            }

            mOpenSSLCTX = SSL_CTX_new(SSLv23_method());
            SSL_CTX_set_client_CA_list(mOpenSSLCTX, 
                SSL_load_client_CA_file(certificate.c_str()));
            SSL_CTX_set_verify_depth(mOpenSSLCTX, 10);

            if (SSL_CTX_use_certificate_chain_file(mOpenSSLCTX, 
                certificate.c_str()) <= 0)
            {
                SSL_CTX_free(mOpenSSLCTX);
                mOpenSSLCTX = nullptr;
                return false;
            }

            if (SSL_CTX_use_PrivateKey_file(mOpenSSLCTX, 
                privatekey.c_str(), 
                SSL_FILETYPE_PEM) <= 0)
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

        void        destroySSL()
        {
            if (mOpenSSLCTX != nullptr)
            {
                SSL_CTX_free(mOpenSSLCTX);
                mOpenSSLCTX = nullptr;
            }
        }

        SSL_CTX* getOpenSSLCTX()
        {
            return mOpenSSLCTX;
        }
#endif
        static  Ptr Create()
        {
            class make_shared_enabler : public SSLHelper {};
            return std::make_shared<make_shared_enabler>();
        }

    protected:
        SSLHelper() BRYNET_NOEXCEPT
        {
#ifdef BRYNET_USE_OPENSSL
            mOpenSSLCTX = nullptr;
#endif
        }

        virtual ~SSLHelper() BRYNET_NOEXCEPT
        {
#ifdef BRYNET_USE_OPENSSL
            destroySSL();
#endif
        }

    private:
#ifdef BRYNET_USE_OPENSSL
        SSL_CTX*        mOpenSSLCTX;
#endif
    };

} }
