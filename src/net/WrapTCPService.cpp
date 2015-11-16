#include <iostream>
using namespace std;

#include "WrapTCPService.h"

TCPSession::TCPSession()
{
    mSocketID = -1;
    mUserData = -1;
    mCloseCallback = nullptr;
    mDataCallback = nullptr;
    mService = nullptr;
}

TCPSession::~TCPSession()
{
    mSocketID = -1;
    mUserData = -1;
}

int64_t TCPSession::getUD()
{
    return mUserData;
}

void    TCPSession::setUD(int64_t ud)
{
    mUserData = ud;
}

string TCPSession::getIP() const
{
    return mIP;
}

int64_t TCPSession::getSocketID() const
{
    return mSocketID;
}

void    TCPSession::send(const char* buffer, int len)
{
    mService->send(mSocketID, DataSocket::makePacket(buffer, len));
}

void TCPSession::send(const DataSocket::PACKET_PTR& packet)
{
    mService->send(mSocketID, packet);
}

void TCPSession::postClose()
{
    mService->disConnect(mSocketID);
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

void TCPSession::setIP(const string& ip)
{
    mIP = ip;
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
}

WrapServer::~WrapServer()
{
}

TcpService::PTR& WrapServer::getService()
{
    return mTCPService;
}

void    WrapServer::startWorkThread(int threadNum, TcpService::FRAME_CALLBACK callback)
{
    mTCPService->startWorkerThread(threadNum, callback);
}

void    WrapServer::addSession(int fd, SESSION_ENTER_CALLBACK userEnterCallback, bool isUseSSL)
{
    TCPSession::PTR tmpSession = std::make_shared<TCPSession>();
    tmpSession->setService(mTCPService);

    auto enterCallback = [tmpSession, userEnterCallback](int64_t id, std::string ip){
        tmpSession->setSocketID(id);
        tmpSession->setIP(ip);
        if (userEnterCallback != nullptr)
        {
            userEnterCallback(tmpSession);
        }
    };

    auto closeCallback = [tmpSession](int64_t id){
        auto& callback = tmpSession->getCloseCallback();
        if (callback != nullptr)
        {
            callback(tmpSession);
        }
    };

    auto msgCallback = [tmpSession](int64_t id, const char* buffer, int len){
        auto& callback = tmpSession->getDataCallback();
        if (callback != nullptr)
        {
            return callback(tmpSession, buffer, len);
        }
        else
        {
            return 0;
        }
    };

    mTCPService->addDataSocket(fd, enterCallback, closeCallback, msgCallback, isUseSSL);
}