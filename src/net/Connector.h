#ifndef DODO_NET_CONNECTOR_H_
#define DODO_NET_CONNECTOR_H_

#include <functional>
#include <memory>
#include <any>

#include "EventLoop.h"
#include "NonCopyable.h"
#include "SocketLibTypes.h"

namespace brynet
{
    namespace net
    {
        class ConnectorWorkThread;
        class AsyncConnectAddr;

        class ThreadConnector : NonCopyable, public std::enable_shared_from_this<ThreadConnector>
        {
        public:
            typedef std::shared_ptr<ThreadConnector> PTR;
            /*  sock为-1表示失败, TODO::可单独添加Failed Callback   */
            typedef std::function<void(sock, const std::any&)> COMPLETED_CALLBACK;

            void                startThread(COMPLETED_CALLBACK callback);
            void                destroy();
            void                asyncConnect(const char* ip, int port, int ms, std::any ud);

            static  PTR         Create();

        private:
            ThreadConnector();
            virtual ~ThreadConnector();
            void                run(std::shared_ptr<ConnectorWorkThread>);

        private:
            std::shared_ptr<MsgQueue<AsyncConnectAddr>>      mConnectRequests;     /*  请求列表    */
            EventLoop                       mEventLoop;

            std::shared_ptr<std::thread>    mThread;
            bool                            mIsRun;
        };
    }
}

#endif