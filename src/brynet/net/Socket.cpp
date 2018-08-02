#include <brynet/net/Socket.h>

brynet::net::TcpSocket::PTR brynet::net::TcpSocket::Create(
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

sock brynet::net::TcpSocket::getFD() const
{
    return mFD;
}

bool brynet::net::TcpSocket::isServerSide() const
{
    return mServerSide;
}

void brynet::net::TcpSocket::SocketNodelay() const
{
    base::SocketNodelay(mFD);
}

bool brynet::net::TcpSocket::SocketNonblock() const
{
    return base::SocketNonblock(mFD);
}

void brynet::net::TcpSocket::SetSendSize(int sdSize) const
{
    base::SocketSetSendSize(mFD, sdSize);
}

void brynet::net::TcpSocket::SetRecvSize(int rdSize) const
{
    base::SocketSetRecvSize(mFD, rdSize);
}

std::string brynet::net::TcpSocket::GetIP() const
{
    return base::GetIPOfSocket(mFD);
}

brynet::net::TcpSocket::TcpSocket(
    sock fd,
    bool serverSide)
    :
    mFD(fd),
    mServerSide(serverSide)
{}

brynet::net::TcpSocket::~TcpSocket()
{
    brynet::net::base::SocketClose(mFD);
}

brynet::net::TcpSocket::PTR brynet::net::ListenSocket::Accept()
{
    sock clientFD = brynet::net::base::Accept(mFD, nullptr, nullptr);
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

brynet::net::ListenSocket::PTR brynet::net::ListenSocket::Create(sock fd)
{
    struct make_unique_enabler : public ListenSocket
    {
    public:
        make_unique_enabler(sock fd) : ListenSocket(fd) {}
    };

    return PTR(new make_unique_enabler(fd));
}

brynet::net::ListenSocket::ListenSocket(sock fd)
    :
    mFD(fd)
{}

brynet::net::ListenSocket::~ListenSocket()
{
    brynet::net::base::SocketClose(mFD);
}