#pragma once

#include <memory>
#include <string>
#include <exception>
#include <stdexcept>

#include <brynet/base/NonCopyable.hpp>
#include <brynet/net/SocketLibFunction.hpp>

namespace brynet { namespace net {

    class TcpConnection;

    class UniqueFd final : public brynet::base::NonCopyable
    {
    public:
        explicit UniqueFd(BrynetSocketFD fd)
            :
            mFD(fd)
        {}

        ~UniqueFd()
        {
            brynet::net::base::SocketClose(mFD);
        }

        UniqueFd(const UniqueFd& other) = delete;
        UniqueFd& operator=(const UniqueFd& other) = delete;

        BrynetSocketFD    getFD() const
        {
            return mFD;
        }

    private:
        BrynetSocketFD    mFD;
    };

    class TcpSocket : public brynet::base::NonCopyable
    {
    private:
        class TcpSocketDeleter
        {
        public:
            void operator()(TcpSocket* ptr) const
            {
                delete ptr;
            }
        };
    public:
        using Ptr = std::unique_ptr<TcpSocket, TcpSocketDeleter>;

    public:
        static Ptr Create(BrynetSocketFD fd, bool serverSide)
        {
            class make_unique_enabler : public TcpSocket
            {
            public:
                make_unique_enabler(BrynetSocketFD fd, bool serverSide)
                    :
                    TcpSocket(fd, serverSide)
                    {}
            };

            return Ptr(new make_unique_enabler(fd, serverSide));
        }

    public:
        void            setNodelay() const
        {
            brynet::net::base::SocketNodelay(mFD);
        }

        bool            setNonblock() const
        {
            return brynet::net::base::SocketNonblock(mFD);
        }

        void            setSendSize(int sdSize) const
        {
            brynet::net::base::SocketSetSendSize(mFD, sdSize);
        }

        void            setRecvSize(int rdSize) const
        {
            brynet::net::base::SocketSetRecvSize(mFD, rdSize);
        }

        std::string     getRemoteIP() const
        {
            return brynet::net::base::GetIPOfSocket(mFD);
        }

        bool            isServerSide() const
        {
            return mServerSide;
        }

    protected:
        TcpSocket(BrynetSocketFD fd, bool serverSide)
            :
            mFD(fd),
            mServerSide(serverSide)
        {
        }

        virtual ~TcpSocket()
        {
            brynet::net::base::SocketClose(mFD);
        }

        BrynetSocketFD    getFD() const
        {
            return mFD;
        }

    private:
        const BrynetSocketFD  mFD;
        const bool      mServerSide;

        friend class TcpConnection;
    };

    class EintrError : public std::exception
    {
    };

    class AcceptError : public std::runtime_error
    {
    public:
        explicit AcceptError(int errorCode)
            :
            std::runtime_error(std::to_string(errorCode)),
            mErrorCode(errorCode)
        {}

        int getErrorCode() const
        {
            return mErrorCode;
        }

    private:
        int mErrorCode;
    };

    class ListenSocket : public brynet::base::NonCopyable
    {
    private:
        class ListenSocketDeleter
        {
        public:
            void operator()(ListenSocket* ptr) const
            {
                delete ptr;
            }
        };
    public:
        using Ptr = std::unique_ptr<ListenSocket, ListenSocketDeleter>;

    public:
        TcpSocket::Ptr accept()
        {
            const auto clientFD = brynet::net::base::Accept(mFD, nullptr, nullptr);
            if (clientFD == BRYNET_INVALID_SOCKET)
            {
#if defined BRYNET_PLATFORM_LINUX || defined BRYNET_PLATFORM_DARWIN
                if (BRYNET_ERRNO == EMFILE)
                {
                    // Thanks libev and muduo.
                    // Read the section named "The special problem of
                    // accept()ing when you can't" in libev's doc.
                    // By Marc Lehmann, author of libev.
                    mIdle.reset();
                    TcpSocket::Create(brynet::net::base::Accept(mFD, nullptr, nullptr), true);
                    mIdle = brynet::net::TcpSocket::Create(::open("/dev/null", O_RDONLY | O_CLOEXEC), true);
                }
#endif
                if (BRYNET_ERRNO == EINTR)
                {
                    throw EintrError();
                }
                else
                {
                    throw AcceptError(BRYNET_ERRNO);
                }
            }

            return TcpSocket::Create(clientFD, true);
        }

    public:
        static Ptr Create(BrynetSocketFD fd)
        {
            class make_unique_enabler : public ListenSocket
            {
            public:
                explicit make_unique_enabler(BrynetSocketFD fd)
                    : ListenSocket(fd) 
                {}
            };

            return Ptr(new make_unique_enabler(fd));
        }

    protected:
        explicit ListenSocket(BrynetSocketFD fd)
            :
            mFD(fd)
        {
#if defined BRYNET_PLATFORM_LINUX || defined BRYNET_PLATFORM_DARWIN
            mIdle = brynet::net::TcpSocket::Create(::open("/dev/null", O_RDONLY | O_CLOEXEC), true);
#endif
        }

        virtual ~ListenSocket()
        {
            brynet::net::base::SocketClose(mFD);
        }

    private:
        const BrynetSocketFD  mFD;
#if defined BRYNET_PLATFORM_LINUX || defined BRYNET_PLATFORM_DARWIN
        brynet::net::TcpSocket::Ptr mIdle;
#endif

        friend class TcpConnection;
    };

} }