#include <cstdlib>
#include <iostream>

#include <brynet/net/SocketLibFunction.h>
#include <brynet/net/Noexcept.h>
#include <brynet/net/Socket.h>
#include <brynet/net/Wrapper.h>
#include <brynet/net/Connector.h>

#include <brynet/net/ListenThread.h>

namespace brynet { namespace net {

    ListenThread::Ptr ListenThread::Create(bool isIPV6,
        const std::string& ip,
        int port,
        const AccepCallback& callback,
        const std::vector<TcpSocketProcessCallback>& processCallbacks)
    {
        class make_shared_enabler : public ListenThread
        {
        public:
            make_shared_enabler(bool isIPV6,
                const std::string& ip,
                int port,
                const AccepCallback& callback,
                const std::vector<TcpSocketProcessCallback>& processCallbacks)
                :
                ListenThread(isIPV6, ip, port, callback, processCallbacks)
            {}
        };
        return std::make_shared<make_shared_enabler>(isIPV6, ip, port, callback, processCallbacks);
    }

    ListenThread::ListenThread(bool isIPV6,
        const std::string& ip,
        int port,
        const AccepCallback& callback,
        const std::vector<TcpSocketProcessCallback>& processCallbacks)
        :
        mIsIPV6(isIPV6),
        mIP(ip),
        mPort(port),
        mCallback(callback),
        mProcessCallbacks(processCallbacks)
    {
        if (mCallback == nullptr)
        {
            throw BrynetCommonException("accept callback is nullptr");
        }
        mRunListen = std::make_shared<bool>(false);
    }

    ListenThread::~ListenThread() BRYNET_NOEXCEPT
    {
        stopListen();
    }

    static brynet::net::TcpSocket::Ptr runOnceListen(const std::shared_ptr<ListenSocket>& listenSocket)
    {
        try
        {
            auto clientSocket = listenSocket->accept();
            return clientSocket;
        }
        catch (const EintrError& e)
        {
            std::cerr << "accept eintr execption:" << e.what() << std::endl;
        }
        catch (const AcceptError& e)
        {
            std::cerr << "accept execption:" << e.what() << std::endl;
        }

        return nullptr;
    }

    void ListenThread::startListen()
    {
        std::lock_guard<std::mutex> lck(mListenThreadGuard);

        if (mListenThread != nullptr)
        {
            return;
        }

        const sock fd = brynet::net::base::Listen(mIsIPV6, mIP.c_str(), mPort, 512);
        if (fd == INVALID_SOCKET)
        {
            throw BrynetCommonException(std::string("listen error of:") + std::to_string(sErrno));
        }

        mRunListen = std::make_shared<bool>(true);

        auto listenSocket = std::shared_ptr<ListenSocket>(ListenSocket::Create(fd));
        auto isRunListen = mRunListen;
        auto callback = mCallback;
        auto processCallbacks = mProcessCallbacks;
        mListenThread = std::make_shared<std::thread>([isRunListen, listenSocket, callback, processCallbacks]() mutable {
            while (*isRunListen)
            {
                auto clientSocket = runOnceListen(listenSocket);
                if (clientSocket == nullptr)
                {
                    continue;
                }

                if (*isRunListen)
                {
                    for (const auto& process : processCallbacks)
                    {
                        process(*clientSocket);
                    }
                    callback(std::move(clientSocket));
                }
            }
        });
    }

    void ListenThread::stopListen()
    {
        std::lock_guard<std::mutex> lck(mListenThreadGuard);

        if (mListenThread == nullptr)
        {
            return;
        }

        *mRunListen = false;
        auto selfIP = mIP;
        if (selfIP == "0.0.0.0")
        {
            selfIP = "127.0.0.1";
        }
        
        auto connector = AsyncConnector::Create();
        connector->startWorkerThread();

        wrapper::SocketConnectBuilder connectBuilder;
        connectBuilder
            .configureConnector(connector)
            .configureConnectOptions({
                AsyncConnector::ConnectOptions::WithTimeout(std::chrono::seconds(2)),
                AsyncConnector::ConnectOptions::WithAddr(selfIP, mPort)
            })
            .syncConnect();

        try
        {
            if (mListenThread->joinable())
            {
                mListenThread->join();
            }
        }
        catch (std::system_error& e)
        {
            (void)e;
        }
        mListenThread = nullptr;
    }

} }