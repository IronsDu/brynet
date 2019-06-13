#pragma once

#include <memory>
#include <string>
#include <exception>
#include <stdexcept>

#include <brynet/utils/NonCopyable.h>
#include <brynet/net/SocketLibFunction.h>

namespace brynet { namespace net {

    class TcpConnection;

    class UniqueFd final : public utils::NonCopyable
    {
    public:
        explicit UniqueFd(sock fd);
        ~UniqueFd();

        UniqueFd(const UniqueFd& other) = delete;
        UniqueFd& operator=(const UniqueFd& other) = delete;

        sock            getFD() const;

    private:
        sock            mFD;
    };

    class TcpSocket : public utils::NonCopyable
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
        static Ptr Create(sock fd, bool serverSide);

    public:
        void            setNodelay() const;
        bool            setNonblock() const;
        void            setSendSize(int sdSize) const;
        void            setRecvSize(int rdSize) const;
        std::string     getRemoteIP() const;
        bool            isServerSide() const;

    protected:
        TcpSocket(sock fd, bool serverSide);
        virtual ~TcpSocket();

        sock            getFD() const;

    private:
        const sock      mFD;
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

    class ListenSocket : public utils::NonCopyable
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
        TcpSocket::Ptr accept();

    public:
        static Ptr Create(sock fd);

    protected:
        explicit ListenSocket(sock fd);
        virtual ~ListenSocket();

    private:
        const sock  mFD;

        friend class TcpConnection;
    };

} }