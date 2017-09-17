#ifndef BRYNET_NET_WRAPTCPSERVICE_H_
#define BRYNET_NET_WRAPTCPSERVICE_H_

#include <string>
#include <cstdint>

#include <brynet/net/TCPService.h>
#include <brynet/utils/NonCopyable.h>
#include <brynet/net/Any.h>

namespace brynet
{
    namespace net
    {
        class WrapTcpService;
        class IOLoopData;

        /*  以智能指针对象为网络会话标识的网络服务   */
        class TCPSession : public NonCopyable
        {
        public:
            typedef std::shared_ptr<TCPSession>     PTR;
            typedef std::weak_ptr<TCPSession>       WEAK_PTR;

            typedef std::function<void(const TCPSession::PTR&)>   CLOSE_CALLBACK;
            typedef std::function<size_t(const TCPSession::PTR&, const char*, size_t)>   DATA_CALLBACK;

        public:
            const BrynetAny&            getUD() const;
            void                        setUD(BrynetAny ud);

            const std::string&          getIP() const;
            TcpService::SESSION_TYPE    getSocketID() const;

            void                        send(const char* buffer, 
                                             size_t len, const 
                                             DataSocket::PACKED_SENDED_CALLBACK& callback = nullptr) const;

            void                        send(const DataSocket::PACKET_PTR& packet, 
                                             const DataSocket::PACKED_SENDED_CALLBACK& callback = nullptr) const;

            void                        postShutdown() const;
            void                        postClose() const;

            void                        setCloseCallback(CLOSE_CALLBACK callback);
            void                        setDataCallback(DATA_CALLBACK callback);
            const EventLoop::PTR&       getEventLoop() const;

            void                        setPingCheckTime(std::chrono::nanoseconds checkTime);

        private:
            TCPSession() noexcept;
            virtual ~TCPSession() noexcept;

            void                        setSocketID(TcpService::SESSION_TYPE id);
            void                        setIP(const std::string& ip);

            void                        setIOLoopData(std::shared_ptr<IOLoopData> ioLoopData);
            void                        setService(const TcpService::PTR& service);

            const CLOSE_CALLBACK&       getCloseCallback();
            const DATA_CALLBACK&        getDataCallback();

            static  PTR                 Create();

        private:
            TcpService::PTR             mService;
            std::shared_ptr<IOLoopData> mIoLoopData;
            TcpService::SESSION_TYPE    mSocketID;
            std::string                 mIP;
            BrynetAny                   mUD;

            CLOSE_CALLBACK              mCloseCallback;
            DATA_CALLBACK               mDataCallback;

            friend class WrapTcpService;
        };

        class WrapTcpService : public NonCopyable
        {
        public:
            typedef std::shared_ptr<WrapTcpService> PTR;
            typedef std::weak_ptr<WrapTcpService>   WEAK_PTR;

            typedef std::function<void(const TCPSession::PTR&)>   SESSION_ENTER_CALLBACK;

            WrapTcpService() noexcept;
            virtual ~WrapTcpService() noexcept;

            void                        startWorkThread(size_t threadNum, 
                                                        TcpService::FRAME_CALLBACK callback = nullptr);
            void                        stopWorkThread();
            
            //TODO::改掉 ssl和listenThread的使用方式
            void                        addSession(sock fd, 
                                                    const SESSION_ENTER_CALLBACK& userEnterCallback, 
                                                    bool isUseSSL,
                                                    const std::shared_ptr<ListenThread>& listenThread,
                                                    size_t maxRecvBufferSize, 
                                                    bool forceSameThreadLoop = false);

        private:
            TcpService::PTR             mTCPService;
        };
    }
}

#endif
