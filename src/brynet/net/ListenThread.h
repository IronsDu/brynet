#pragma once

#include <string>
#include <functional>
#include <thread>
#include <memory>
#include <mutex>
#include <vector>

#include <brynet/utils/NonCopyable.h>
#include <brynet/utils/Typeids.h>
#include <brynet/net/SocketLibFunction.h>
#include <brynet/net/Noexcept.h>
#include <brynet/net/Socket.h>

namespace brynet { namespace net {

    class ListenThread : public utils::NonCopyable, public std::enable_shared_from_this<ListenThread>
    {
    public:
        using Ptr = std::shared_ptr<ListenThread>;
        using AccepCallback = std::function<void(TcpSocket::Ptr)>;
        using TcpSocketProcessCallback = std::function<void(TcpSocket&)>;

        void                                startListen();
        void                                stopListen();

    public:
        static  Ptr                         Create(bool isIPV6,
                                                const std::string& ip,
                                                int port,
                                                const AccepCallback& callback,
                                                const std::vector<TcpSocketProcessCallback>& = {});

    private:
        ListenThread(bool isIPV6, 
            const std::string& ip, 
            int port, 
            const AccepCallback& callback,
            const std::vector<TcpSocketProcessCallback>& processCallbacks);
        virtual ~ListenThread() BRYNET_NOEXCEPT;

    private:
        const bool                          mIsIPV6;
        const std::string                   mIP;
        const int                           mPort;
        const AccepCallback                 mCallback;
        const std::vector<TcpSocketProcessCallback>    mProcessCallbacks;

        std::shared_ptr<bool>               mRunListen;
        std::shared_ptr<std::thread>        mListenThread;
        std::mutex                          mListenThreadGuard;
    };

} }
