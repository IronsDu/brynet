#include <cstdlib>
#include <brynet/net/SocketLibFunction.h>

#include <brynet/net/ListenThread.h>

using namespace brynet::net;

ListenThread::PTR ListenThread::Create()
{
    struct make_shared_enabler : public ListenThread {};
    return std::make_shared<make_shared_enabler>();
}

ListenThread::ListenThread() noexcept
{
    mIsIPV6 = false;
    mAcceptCallback = nullptr;
    mPort = 0;
    mRunListen = false;
#ifdef USE_OPENSSL
    mOpenSSLCTX = nullptr;
#endif
}

ListenThread::~ListenThread() noexcept
{
    closeListenThread();
}

void ListenThread::startListen(bool isIPV6, 
    const std::string& ip,
    int port,
    ACCEPT_CALLBACK callback)
{
    std::lock_guard<std::mutex> lck(mListenThreadGuard);

    if (mListenThread != nullptr)
    {
        return;
    }

    mIsIPV6 = isIPV6;
    mRunListen = true;
    mIP = ip;
    mPort = port;
    mAcceptCallback = callback;

    mListenThread = std::make_shared<std::thread>([shared_this = shared_from_this()](){
        shared_this->runListen();
    });
}

void ListenThread::closeListenThread()
{
    std::lock_guard<std::mutex> lck(mListenThreadGuard);

    if (mListenThread == nullptr)
    {
        return;
    }

    mRunListen = false;

    sock tmp = ox_socket_connect(mIsIPV6, mIP.c_str(), mPort);
    ox_socket_close(tmp);
    tmp = SOCKET_ERROR;

    if (mListenThread->joinable())
    {
        mListenThread->join();
    }
    mListenThread = nullptr;
}

#ifdef USE_OPENSSL
SSL_CTX* ListenThread::getOpenSSLCTX()
{
    return mOpenSSLCTX;
}

bool ListenThread::initSSL(const std::string& certificate, const std::string& privatekey)
{
    if (mOpenSSLCTX != nullptr)
    {
        return false;
    }
    if (certificate.empty() || privatekey.empty())
    {
        return false;
    }

    mOpenSSLCTX = SSL_CTX_new(SSLv23_server_method());
    if (SSL_CTX_use_certificate_file(mOpenSSLCTX, certificate.c_str(), SSL_FILETYPE_PEM) <= 0)
    {
        SSL_CTX_free(mOpenSSLCTX);
        mOpenSSLCTX = nullptr;
    }
    /* 载入用户私钥 */
    if (SSL_CTX_use_PrivateKey_file(mOpenSSLCTX, privatekey.c_str(), SSL_FILETYPE_PEM) <= 0)
    {
        SSL_CTX_free(mOpenSSLCTX);
        mOpenSSLCTX = nullptr;
    }
    /* 检查用户私钥是否正确 */
    if (!SSL_CTX_check_private_key(mOpenSSLCTX))
    {
        SSL_CTX_free(mOpenSSLCTX);
        mOpenSSLCTX = nullptr;
    }

    return true;
}

void ListenThread::destroySSL()
{
    if(mOpenSSLCTX != nullptr)
    {
        SSL_CTX_free(mOpenSSLCTX);
        mOpenSSLCTX = nullptr;
    }
}
#endif

void ListenThread::runListen()
{
    sock client_fd = SOCKET_ERROR;
    struct sockaddr_in socketaddress;
    struct sockaddr_in6 ip6Addr;
    socklen_t addrLen = sizeof(struct sockaddr);
    sockaddr_in* pAddr = &socketaddress;

    if (mIsIPV6)
    {
        addrLen = sizeof(ip6Addr);
        pAddr = (sockaddr_in*)&ip6Addr;
    }

    sock listen_fd = ox_socket_listen(mIsIPV6, mIP.c_str(), mPort, 512);
    if (SOCKET_ERROR == listen_fd)
    {
        fprintf(stderr, "listen failed, error:%d \n", sErrno);
        return;
    }

    fprintf(stderr, "listen : %d \n", mPort);
    for (; mRunListen;)
    {
        while ((client_fd = ox_socket_accept(listen_fd, (struct sockaddr*)pAddr, &addrLen)) == SOCKET_ERROR)
        {
            if (EINTR == sErrno)
            {
                continue;
            }
        }

        if (SOCKET_ERROR == client_fd)
        {
            continue;
        }
        if (!mRunListen)
        {
            ox_socket_close(client_fd);
            continue;
        }

        mAcceptCallback(client_fd);
    }

    ox_socket_close(listen_fd);
    listen_fd = SOCKET_ERROR;
}