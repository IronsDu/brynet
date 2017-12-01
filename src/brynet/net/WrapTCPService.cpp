#include <brynet/net/WrapTCPService.h>

using namespace brynet::net;

TCPSession::TCPSession() BRYNET_NOEXCEPT
{
    mSocketID = -1;
    mDisConnectCallback = nullptr;
    mDataCallback = nullptr;
    mService = nullptr;
}

TCPSession::~TCPSession() BRYNET_NOEXCEPT
{
    mSocketID = -1;
}

const BrynetAny& TCPSession::getUD() const
{
    return mUD;
}

void TCPSession::setUD(BrynetAny ud)
{
    mUD = std::move(ud);
}

const std::string& TCPSession::getIP() const
{
    return mIP;
}

TcpService::SESSION_TYPE TCPSession::getSocketID() const
{
    return mSocketID;
}

void TCPSession::send(const char* buffer, 
    size_t len, 
    const DataSocket::PACKED_SENDED_CALLBACK& callback) const
{
    IOLoopDataSend(mIoLoopData, mSocketID, DataSocket::makePacket(buffer, len), callback);
}

void TCPSession::send(const DataSocket::PACKET_PTR& packet, 
    const DataSocket::PACKED_SENDED_CALLBACK& callback) const
{
    IOLoopDataSend(mIoLoopData, mSocketID, packet, callback);
}

void TCPSession::postShutdown() const
{
    mService->postShutdown(mSocketID);
}

void TCPSession::postDisConnect() const
{
    mService->postDisConnect(mSocketID);
}

void TCPSession::setDisConnectCallback(DISCONNECT_CALLBACK callback)
{
    mDisConnectCallback = std::move(callback);
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
    return IOLoopDataGetEventLoop(mIoLoopData);
}

void TCPSession::setHeartBeat(std::chrono::nanoseconds checkTime)
{
    mService->setHeartBeat(mSocketID, checkTime);
}

void TCPSession::setSocketID(TcpService::SESSION_TYPE id)
{
    mSocketID = id;
}

void TCPSession::setIP(const std::string& ip)
{
    mIP = ip;
}

void TCPSession::setService(const TcpService::PTR& service)
{
    mService = service;
}

const TCPSession::DISCONNECT_CALLBACK& TCPSession::getCloseCallback()
{
    return mDisConnectCallback;
}

const TCPSession::DATA_CALLBACK& TCPSession::getDataCallback()
{
    return mDataCallback;
}

TCPSession::PTR TCPSession::Create()
{
    struct make_shared_enabler : public TCPSession {};
    return std::make_shared<make_shared_enabler>();
}

WrapTcpService::WrapTcpService() BRYNET_NOEXCEPT
{
    mTCPService = TcpService::Create();
}

WrapTcpService::~WrapTcpService() BRYNET_NOEXCEPT
{
    stopWorkThread();
}

void WrapTcpService::stopWorkThread()
{
    mTCPService->stopWorkerThread();
}

void WrapTcpService::startWorkThread(size_t threadNum, TcpService::FRAME_CALLBACK callback)
{
    mTCPService->startWorkerThread(threadNum, callback);
}

void WrapTcpService::addSession(sock fd, 
    const SESSION_ENTER_CALLBACK& userEnterCallback, 
    bool isUseSSL,
    const SSLHelper::PTR& sslHelper,
    size_t maxRecvBufferSize, 
    bool forceSameThreadLoop)
{
    auto tmpSession = TCPSession::Create();
    tmpSession->setService(mTCPService);

    auto sharedTCPService = mTCPService;
    auto enterCallback = [sharedTCPService, 
        tmpSession, 
        userEnterCallback](TcpService::SESSION_TYPE id, 
                            const std::string& ip) mutable {
        tmpSession->setIOLoopData(sharedTCPService->getIOLoopDataBySocketID(id));
        tmpSession->setSocketID(id);
        tmpSession->setIP(ip);
        if (userEnterCallback != nullptr)
        {
            userEnterCallback(tmpSession);
        }
    };

    auto closeCallback = [tmpSession](TcpService::SESSION_TYPE id) mutable {
        const auto& callback = tmpSession->getCloseCallback();
        if (callback != nullptr)
        {
            callback(tmpSession);
        }
    };

    auto msgCallback = [tmpSession](TcpService::SESSION_TYPE id, 
        const char* buffer, 
        size_t len) mutable {
        const auto& callback = tmpSession->getDataCallback();
        if (callback != nullptr)
        {
            return callback(tmpSession, buffer, len);
        }
        else
        {
            return static_cast<size_t>(0);
        }
    };

    mTCPService->addDataSocket(fd, 
        sslHelper,
        isUseSSL,
        enterCallback, 
        closeCallback, 
        msgCallback, 
        maxRecvBufferSize, 
        forceSameThreadLoop);
}