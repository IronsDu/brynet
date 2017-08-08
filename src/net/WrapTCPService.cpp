#include "WrapTCPService.h"

using namespace brynet::net;

TCPSession::TCPSession() noexcept
{
    mSocketID = -1;
    mCloseCallback = nullptr;
    mDataCallback = nullptr;
    mService = nullptr;
}

TCPSession::~TCPSession() noexcept
{
    mSocketID = -1;
}

const std::any& TCPSession::getUD() const
{
    return mUD;
}

void TCPSession::setUD(std::any ud)
{
    mUD = ud;
}

const std::string& TCPSession::getIP() const
{
    return mIP;
}

TcpService::SESSION_TYPE TCPSession::getSocketID() const
{
    return mSocketID;
}

void TCPSession::send(const char* buffer, size_t len, DataSocket::PACKED_SENDED_CALLBACK callback) const
{
    mIoLoopData->send(mSocketID, DataSocket::makePacket(buffer, len), std::move(callback));
}

void TCPSession::send(DataSocket::PACKET_PTR packet, DataSocket::PACKED_SENDED_CALLBACK callback) const
{
    mIoLoopData->send(mSocketID, std::move(packet), std::move(callback));
}

void TCPSession::postShutdown() const
{
    mService->shutdown(mSocketID);
}

void TCPSession::postClose() const
{
    mService->disConnect(mSocketID);
}

void TCPSession::setCloseCallback(CLOSE_CALLBACK callback)
{
    mCloseCallback = std::move(callback);
}

void TCPSession::setDataCallback(DATA_CALLBACK callback)
{
    mDataCallback = std::move(callback);
}

void TCPSession::setIOLoopData(std::shared_ptr<IOLoopData> ioLoopData)
{
    mIoLoopData = std::move(ioLoopData);
}

const EventLoop::PTR& TCPSession::getEventLoop() const
{
    return mIoLoopData->getEventLoop();
}

void TCPSession::setSocketID(TcpService::SESSION_TYPE id)
{
    mSocketID = id;
}

void TCPSession::setIP(const std::string& ip)
{
    mIP = ip;
}

void TCPSession::setService(TcpService::PTR& service)
{
    mService = service;
}

TCPSession::CLOSE_CALLBACK& TCPSession::getCloseCallback()
{
    return mCloseCallback;
}

TCPSession::DATA_CALLBACK& TCPSession::getDataCallback()
{
    return mDataCallback;
}

TCPSession::PTR TCPSession::Create()
{
    struct make_shared_enabler : public TCPSession {};
    return std::make_shared<make_shared_enabler>();
}

WrapTcpService::WrapTcpService() noexcept
{
    mTCPService = TcpService::Create();
}

WrapTcpService::~WrapTcpService() noexcept
{
}

const TcpService::PTR& WrapTcpService::getService() const
{
    return mTCPService;
}

void WrapTcpService::startWorkThread(size_t threadNum, TcpService::FRAME_CALLBACK callback)
{
    mTCPService->startWorkerThread(threadNum, callback);
}

void WrapTcpService::addSession(sock fd, const SESSION_ENTER_CALLBACK& userEnterCallback, bool isUseSSL, size_t maxRecvBufferSize, bool forceSameThreadLoop)
{
    auto tmpSession = TCPSession::Create();
    tmpSession->setService(mTCPService);

    auto enterCallback = [sharedTCPService = mTCPService, tmpSession, userEnterCallback](TcpService::SESSION_TYPE id, const std::string& ip) mutable {
        tmpSession->setIOLoopData(sharedTCPService->getIOLoopDataBySocketID(id));
        tmpSession->setSocketID(id);
        tmpSession->setIP(ip);
        if (userEnterCallback != nullptr)
        {
            userEnterCallback(tmpSession);
        }
    };

    auto closeCallback = [tmpSession](TcpService::SESSION_TYPE id) mutable {
        auto& callback = tmpSession->getCloseCallback();
        if (callback != nullptr)
        {
            callback(tmpSession);
        }
    };

    auto msgCallback = [tmpSession](TcpService::SESSION_TYPE id, const char* buffer, size_t len) mutable {
        auto& callback = tmpSession->getDataCallback();
        if (callback != nullptr)
        {
            return callback(tmpSession, buffer, len);
        }
        else
        {
            return static_cast<size_t>(0);
        }
    };

    mTCPService->addDataSocket(fd, enterCallback, closeCallback, msgCallback, isUseSSL, maxRecvBufferSize, forceSameThreadLoop);
}