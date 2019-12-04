#pragma once

#include <brynet/net/TcpService.hpp>
#include <brynet/net/ListenThread.hpp>
#include <brynet/net/Exception.hpp>

namespace brynet { namespace net { namespace wrapper {

    class ListenConfig final
    {
    public:
        ListenConfig()
        {
            mSetting = false;
            mIsIpV6 = false;
            mPort = 0;
        }

        void        setAddr(bool ipV6, std::string ip, int port)
        {
            mIsIpV6 = ipV6;
            mListenAddr = ip;
            mPort = port;
            mSetting = true;
        }

        std::string ip() const
        {
            return mListenAddr;
        }

        int         port() const
        {
            return mPort;
        }

        bool        useIpV6() const
        {
            return mIsIpV6;
        }

        bool        hasSetting() const
        {
            return mSetting;
        }

    private:
        std::string mListenAddr;
        int         mPort;
        bool        mIsIpV6;
        bool        mSetting;
    };

    class BuildListenConfig
    {
    public:
        explicit BuildListenConfig(ListenConfig* config)
            :
            mConfig(config)
        {
        }

        void        setAddr(bool ipV6, std::string ip, int port)
        {
            mConfig->setAddr(ipV6, ip, port);
        }

    private:
        ListenConfig* mConfig;
    };

    template<typename Derived>
    class BaseListenerBuilder
    {
    protected:
        using AddSocketOptionFunc = detail::AddSocketOptionFunc;
        using ConnectOptionFunc = detail::ConnectOptionFunc;

    public:
        virtual ~BaseListenerBuilder() = default;

        Derived& configureService(TcpService::Ptr service)
        {
            mTcpService = std::move(service);
            return static_cast<Derived&>(*this);
        }

        Derived& configureSocketOptions(std::vector<ListenThread::TcpSocketProcessCallback> options)
        {
            mSocketOptions = std::move(options);
            return static_cast<Derived&>(*this);
        }

        Derived& configureConnectionOptions(std::vector<AddSocketOptionFunc> options)
        {
            mConnectionOptions = std::move(options);
            return static_cast<Derived&>(*this);
        }

        template<typename BuilderFunc>
        Derived& configureListen(const BuilderFunc& builder)
        {
            BuildListenConfig buildConfig(&mListenConfig);
            builder(buildConfig);
            return static_cast<Derived&>(*this);
        }

        void asyncRun()
        {
            asyncRun(getConnectionOptions());
        }

        void    stop()
        {
            if (mListenThread)
            {
                mListenThread->stopListen();
            }
        }

        std::vector<AddSocketOptionFunc>   getConnectionOptions() const
        {
            return mConnectionOptions;
        }

    protected:
        void asyncRun(std::vector<AddSocketOptionFunc> connectionOptions)
        {
            if (mTcpService == nullptr)
            {
                throw BrynetCommonException("tcp service is nullptr");
            }
            if (connectionOptions.empty())
            {
                throw BrynetCommonException("options is empty");
            }
            if (!mListenConfig.hasSetting())
            {
                throw BrynetCommonException("not config listen addr");
            }

            auto service = mTcpService;
            mListenThread = ListenThread::Create(mListenConfig.useIpV6(),
                mListenConfig.ip(),
                mListenConfig.port(),
                [service, connectionOptions](brynet::net::TcpSocket::Ptr socket) {
                    service->addTcpConnection(std::move(socket), connectionOptions);
                },
                mSocketOptions);
            mListenThread->startListen();
        }

    private:
        TcpService::Ptr                                     mTcpService;
        std::vector<ListenThread::TcpSocketProcessCallback> mSocketOptions;
        ListenConfig                                        mListenConfig;
        ListenThread::Ptr                                   mListenThread;

    private:
        std::vector<AddSocketOptionFunc>                    mConnectionOptions;
    };

    class ListenerBuilder : public BaseListenerBuilder<ListenerBuilder>
    {
    };

} } }