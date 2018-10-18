#include <brynet/net/Socket.h>

namespace brynet { namespace net {

    TcpSocket::PTR TcpSocket::Create(
        sock fd,
        bool serverSide)
    {
        struct make_unique_enabler : public TcpSocket
        {
        public:
            make_unique_enabler(sock fd, bool serverSide)
                :
                TcpSocket(fd, serverSide) {}
        };

        return PTR(new make_unique_enabler(fd, serverSide));
    }

    sock TcpSocket::getFD() const
    {
        return mFD;
    }

    bool TcpSocket::isServerSide() const
    {
        return mServerSide;
    }

    void TcpSocket::SocketNodelay() const
    {
        base::SocketNodelay(mFD);
    }

    bool TcpSocket::SocketNonblock() const
    {
        return base::SocketNonblock(mFD);
    }

    void TcpSocket::SetSendSize(int sdSize) const
    {
        base::SocketSetSendSize(mFD, sdSize);
    }

    void TcpSocket::SetRecvSize(int rdSize) const
    {
        base::SocketSetRecvSize(mFD, rdSize);
    }

    std::string TcpSocket::GetIP() const
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

    TcpSocket::PTR ListenSocket::Accept()
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

    ListenSocket::PTR ListenSocket::Create(sock fd)
    {
        struct make_unique_enabler : public ListenSocket
        {
        public:
            make_unique_enabler(sock fd) : ListenSocket(fd) {}
        };

        return PTR(new make_unique_enabler(fd));
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