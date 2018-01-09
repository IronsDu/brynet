#include <cstdlib>
#include <iostream>
#include <brynet/net/SocketLibFunction.h>
#include <brynet/net/Noexcept.h>
#include <brynet/net/Socket.h>

#include <brynet/net/ListenThread.h>

using namespace brynet::net;

ListenThread::PTR ListenThread::Create()
{
    struct make_shared_enabler : public ListenThread {};
    return std::make_shared<make_shared_enabler>();
}

ListenThread::ListenThread() BRYNET_NOEXCEPT
{
    mIsIPV6 = false;
    mAcceptCallback = nullptr;
    mPort = 0;
    mRunListen = false;
}

ListenThread::~ListenThread() BRYNET_NOEXCEPT
{
    stopListen();
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
    if (callback == nullptr)
    {
        throw std::runtime_error("accept callback is nullptr");
    }

    sock fd = brynet::net::base::Listen(isIPV6, ip.c_str(), port, 512);
    if (fd == SOCKET_ERROR)
    {
        throw std::runtime_error("listen error of:" + sErrno);
    }

    mIsIPV6 = isIPV6;
    mRunListen = true;
    mIP = ip;
    mPort = port;
    mAcceptCallback = callback;
    auto shared_this = shared_from_this();
    mListenSocket = ListenSocket::Create(fd);
    mListenThread = std::make_shared<std::thread>([shared_this]() mutable {
        shared_this->runListen();
    });
}

void ListenThread::stopListen()
{
    std::lock_guard<std::mutex> lck(mListenThreadGuard);

    if (mListenThread == nullptr)
    {
        return;
    }

    mRunListen = false;

    sock tmp = brynet::net::base::Connect(mIsIPV6, mIP.c_str(), mPort);
    auto clientSocket = TcpSocket::Create(tmp, false);

    if (mListenThread->joinable())
    {
        mListenThread->join();
    }
    mListenThread = nullptr;
}

void ListenThread::runListen()
{
    for (; mRunListen;)
    {
        try
        {
            auto clientSocket = mListenSocket->Accept();
            if (!mRunListen)
            {
                break;
            }

            mAcceptCallback(std::move(clientSocket));
        }
        catch (const EintrError&)
        {
        }
        catch (const AcceptError& e)
        {
            std::cerr << "accept execption:" << e.what() << std::endl;
            break;
        }
        catch (...)
        {
            std::cerr << "accept unknow execption:" << std::endl;
            break;
        }
    }
}