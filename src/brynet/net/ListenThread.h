#pragma once

#include <string>
#include <functional>
#include <thread>
#include <memory>
#include <mutex>

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

        void                                startListen(bool isIPV6,
                                                const std::string& ip,
                                                int port,
                                                AccepCallback callback);
        void                                stopListen();
        static  Ptr                         Create();

    private:
        ListenThread() BRYNET_NOEXCEPT;
        virtual ~ListenThread() BRYNET_NOEXCEPT;

    private:
        bool                                mIsIPV6;
        std::string                         mIP;
        int                                 mPort;
        std::shared_ptr<bool>               mRunListen;
        std::shared_ptr<std::thread>        mListenThread;
        std::mutex                          mListenThreadGuard;
    };

} }
