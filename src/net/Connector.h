#ifndef BRYNET_NET_CONNECTOR_H_
#define BRYNET_NET_CONNECTOR_H_

#include <functional>
#include <memory>
#include <any>

#include "EventLoop.h"
#include "NonCopyable.h"
#include "SocketLibTypes.h"
#include "MsgQueue.h"

namespace brynet
{
    namespace net
    {
        class ConnectorWorkThread;
        class AsyncConnectAddr;

        class AsyncConnector : NonCopyable, public std::enable_shared_from_this<AsyncConnector>
        {
        public:
            typedef std::shared_ptr<AsyncConnector> PTR;
            typedef std::function<void(sock, const std::any&)> COMPLETED_CALLBACK;
            typedef std::function<void(const std::any&)> FAILED_CALLBACK;

            void                startThread(COMPLETED_CALLBACK completedCallback, FAILED_CALLBACK failedCallback);
            void                destroy();
            void                asyncConnect(const char* ip, int port, int ms, std::any ud);

            static  PTR         Create();

        private:
            AsyncConnector();
            virtual ~AsyncConnector();
            void                run(std::shared_ptr<ConnectorWorkThread>);

        private:
            std::shared_ptr<MsgQueue<AsyncConnectAddr>>      mConnectRequests;     /*  «Î«Û¡–±Ì    */
            EventLoop                       mEventLoop;

            std::shared_ptr<std::thread>    mThread;
            std::mutex                      mThreadGuard;
            bool                            mIsRun;
        };
    }
}

#endif