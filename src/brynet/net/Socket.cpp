#include <brynet/net/Socket.h>

namespace brynet { namespace net {

    UniqueFd::UniqueFd(sock fd)
        :
        mFD(fd)
    {
    }

    UniqueFd::~UniqueFd()
    {
        base::SocketClose(mFD);
    }

    sock UniqueFd::getFD() const
    {
        return mFD;
    }

    TcpSocket::Ptr TcpSocket::Create(
        sock fd,
        bool serverSide)
    {
        class make_unique_enabler : public TcpSocket
        {
        public:
            make_unique_enabler(sock fd, bool serverSide)
                :
                TcpSocket(fd, serverSide) {}
        };

        return Ptr(new make_unique_enabler(fd, serverSide));
    }

    sock TcpSocket::getFD() const
    {
        return mFD;
    }

    bool TcpSocket::isServerSide() const
    {
        return mServerSide;
    }

    void TcpSocket::setNodelay() const
    {
        base::SocketNodelay(mFD);
    }

    bool TcpSocket::setNonblock() const
    {
        return base::SocketNonblock(mFD);
    }

    void TcpSocket::setSendSize(int sdSize) const
    {
        base::SocketSetSendSize(mFD, sdSize);
    }

    void TcpSocket::setRecvSize(int rdSize) const
    {
        base::SocketSetRecvSize(mFD, rdSize);
    }

    std::string TcpSocket::getRemoteIP() const
    {
        return base::GetIPOfSocket(mFD);
    }

    TcpSocket::TcpSocket(
        sock fd,
        bool serverSide)
        :
        mFD(fd),
        mServerSide(serverSide)
    {}

    TcpSocket::~TcpSocket()
    {
        base::SocketClose(mFD);
    }

    TcpSocket::Ptr ListenSocket::accept()
    {
        sock clientFD = base::Accept(mFD, nullptr, nullptr);
        if (clientFD == INVALID_SOCKET)
        {
            if (EINTR == sErrno)
            {
                throw EintrError();
            }
            else
            {
                throw AcceptError(sErrno);
            }
        }

        return TcpSocket::Create(clientFD, true);
    }

    ListenSocket::Ptr ListenSocket::Create(sock fd)
    {
        class make_unique_enabler : public ListenSocket
        {
        public:
            explicit make_unique_enabler(sock fd) : ListenSocket(fd) {}
        };

        return Ptr(new make_unique_enabler(fd));
    }

    ListenSocket::ListenSocket(sock fd)
        :
        mFD(fd)
    {}

    ListenSocket::~ListenSocket()
    {
        base::SocketClose(mFD);
    }

} }