#include <iostream>
using namespace std;

#include "WrapTCPService.h"

TCPSession::TCPSession()
{
    cout << "WrapSession::WrapSession() " << endl;
    mSocketID = -1;
    mUserData = -1;
    mCloseCallback = nullptr;
    mDataCallback = nullptr;
    mService = nullptr;
}

TCPSession::~TCPSession()
{
    cout << "WrapSession::~WrapSession()" << endl;
}

int64_t TCPSession::getUD()
{
    return mUserData;
}

void    TCPSession::setUD(int64_t ud)
{
    mUserData = ud;
}

void    TCPSession::send(const char* buffer, int len)
{
    mService->send(mSocketID, DataSocket::makePacket(buffer, len));
}

void    TCPSession::setCloseCallback(const CLOSE_CALLBACK& callback)
{
    mCloseCallback = callback;
}

void    TCPSession::setDataCallback(const DATA_CALLBACK& callback)
{
    mDataCallback = callback;
}

void    TCPSession::setSocketID(int64_t id)
{
    mSocketID = id;
}

void    TCPSession::setService(TcpService::PTR service)
{
    mService = service;
}

TCPSession::CLOSE_CALLBACK&  TCPSession::getCloseCallback()
{
    return mCloseCallback;
}

TCPSession::DATA_CALLBACK&   TCPSession::getDataCallback()
{
    return mDataCallback;
}

WrapServer::WrapServer()
{
    mTCPService = std::make_shared<TcpService>();
    mSessionDefaultEnterCallback = nullptr;
}

WrapServer::~WrapServer()
{
}

void        WrapServer::setDefaultEnterCallback(SESSION_ENTER_CALLBACK callback)
{
    mSessionDefaultEnterCallback = callback;
}

TcpService::PTR WrapServer::getService()
{
    return mTCPService;
}

void WrapServer::startListen(int port, const char *certificate, const char *privatekey)
{
    auto callback = std::bind(&WrapServer::onAccept, this, std::placeholders::_1);
    mListenThread.startListen(port, certificate, privatekey, callback);
}

void    WrapServer::startWorkThread(int threadNum, TcpService::FRAME_CALLBACK callback)
{
    mTCPService->startWorkerThread(threadNum, callback);
}

void    WrapServer::addSession(int fd, SESSION_ENTER_CALLBACK userEnterCallback)
{
    DataSocket::PTR channel = std::make_shared<DataSocket>(fd);
#ifdef USE_OPENSSL
    if (mListenThread.getOpenSSLCTX() != nullptr)
    {
        channel->setupAcceptSSL(mListenThread.getOpenSSLCTX());
    }
#endif

    TCPSession::PTR tmpSession = std::make_shared<TCPSession>();
    auto enterCallback = [tmpSession, userEnterCallback](int64_t id, std::string){
        tmpSession->setSocketID(id);
        if (userEnterCallback != nullptr)
        {
            userEnterCallback(tmpSession);
        }
    };

    auto closeCallback = [tmpSession](int64_t id){
        auto callback = tmpSession->getCloseCallback();
        if (callback != nullptr)
        {
            callback(tmpSession);
        }
    };

    auto msgCallback = [tmpSession](int64_t id, const char* buffer, int len){
        auto callback = tmpSession->getDataCallback();
        if (callback != nullptr)
        {
            return callback(tmpSession, buffer, len);
        }
        else
        {
            return 0;
        }
    };

    mTCPService->addDataSocket(fd, channel, enterCallback, closeCallback, msgCallback);
}
void    WrapServer::onAccept(int fd)
{
    addSession(fd, mSessionDefaultEnterCallback);
}