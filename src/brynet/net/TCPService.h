#ifndef BRYNET_NET_TCP_SERVICE_H_
#define BRYNET_NET_TCP_SERVICE_H_

#include <vector>
#include <string>
#include <functional>
#include <thread>
#include <cstdint>
#include <memory>

#include <brynet/net/DataSocket.h>
#include <brynet/utils/NonCopyable.h>
#include <brynet/utils/Typeids.h>

namespace brynet
{
    namespace net
    {
        class EventLoop;
        class ListenThread;
        class IOLoopData;

        /*  以数字ID为网络会话标识的网络服务   */
        class TcpService : public NonCopyable, public std::enable_shared_from_this<TcpService>
        {
        public:
            typedef std::shared_ptr<TcpService>                                         PTR;
            typedef int64_t SESSION_TYPE;

            typedef std::function<void(const EventLoop::PTR&)>                          FRAME_CALLBACK;
            typedef std::function<void(SESSION_TYPE, const std::string&)>               ENTER_CALLBACK;
            typedef std::function<void(SESSION_TYPE)>                                   DISCONNECT_CALLBACK;
            typedef std::function<size_t(SESSION_TYPE, const char* buffer, size_t len)> DATA_CALLBACK;

        public:
            static  PTR                         Create();

        public:
            /*  设置默认事件回调    */
            void                                setEnterCallback(TcpService::ENTER_CALLBACK callback);
            void                                setDisconnectCallback(TcpService::DISCONNECT_CALLBACK callback);
            void                                setDataCallback(TcpService::DATA_CALLBACK callback);

            const TcpService::ENTER_CALLBACK&       getEnterCallback() const;
            const TcpService::DISCONNECT_CALLBACK&  getDisconnectCallback() const;
            const TcpService::DATA_CALLBACK&        getDataCallback() const;

            void                                send(SESSION_TYPE id, 
                                                    const DataSocket::PACKET_PTR& packet, 
                                                    const DataSocket::PACKED_SENDED_CALLBACK& callback = nullptr) const;

            void                                shutdown(SESSION_TYPE id) const;

            /*主动断开此id链接，但仍然会收到此id的断开回调，需要上层逻辑自己处理这个"问题"(尽量统一在断开回调函数里做清理等工作) */
            void                                disConnect(SESSION_TYPE id) const;

            void                                setPingCheckTime(SESSION_TYPE id, int checktime);
            
            // TODO:: 改掉 SSL和listenThread的使用
            bool                                addDataSocket(sock fd,
                                                                const std::shared_ptr<ListenThread>& listenThread,
                                                                const TcpService::ENTER_CALLBACK& enterCallback,
                                                                const TcpService::DISCONNECT_CALLBACK& disConnectCallback,
                                                                const TcpService::DATA_CALLBACK& dataCallback,
                                                                bool isUseSSL,
                                                                size_t maxRecvBufferSize,
                                                                bool forceSameThreadLoop = false);

            void                                startWorkerThread(size_t threadNum, FRAME_CALLBACK callback = nullptr);
            void                                stopWorkerThread();

            void                                wakeup(SESSION_TYPE id) const;
            void                                wakeupAll() const;
            EventLoop::PTR                      getRandomEventLoop();
            EventLoop::PTR                      getEventLoopBySocketID(SESSION_TYPE id) const noexcept;
            std::shared_ptr<IOLoopData>         getIOLoopDataBySocketID(SESSION_TYPE id) const noexcept;

        private:
            TcpService() noexcept;
            virtual ~TcpService() noexcept;

            bool                                helpAddChannel(DataSocket::PTR channel,
                                                                const std::string& ip,
                                                                const TcpService::ENTER_CALLBACK& enterCallback,
                                                                const TcpService::DISCONNECT_CALLBACK& disConnectCallback,
                                                                const TcpService::DATA_CALLBACK& dataCallback,
                                                                bool forceSameThreadLoop = false);
        private:
            SESSION_TYPE                        MakeID(size_t loopIndex, const std::shared_ptr<IOLoopData>&);
            void                                procDataSocketClose(DataSocket::PTR);
            /*  对id标识的DataSocket投递一个异步操作(放在网络线程执行)(会验证ID的有效性)  */
            void                                postSessionAsyncProc(SESSION_TYPE id, std::function<void(DataSocket::PTR)> callback) const;

        private:
            std::vector<std::shared_ptr<IOLoopData>>    mIOLoopDatas;
            mutable std::mutex                  mIOLoopGuard;
            bool                                mRunIOLoop;

            std::mutex                          mServiceGuard;

            /*  以下三个回调函数会在多线程中调用(每个线程即一个eventloop驱动的io loop)(见：RunListen中的使用)   */
            TcpService::ENTER_CALLBACK          mEnterCallback;
            TcpService::DISCONNECT_CALLBACK     mDisConnectCallback;
            TcpService::DATA_CALLBACK           mDataCallback;
        };

        void IOLoopDataSend(const std::shared_ptr<IOLoopData>&, TcpService::SESSION_TYPE id, const DataSocket::PACKET_PTR& packet, const DataSocket::PACKED_SENDED_CALLBACK& callback);
        const EventLoop::PTR& IOLoopDataGetEventLoop(const std::shared_ptr<IOLoopData>&);
    }
}

#endif
