#include "WrapTCPService.h"

void onSessionClose(TCPSession::PTR session)
{
    cout << "on session close" << endl;
}

int onSessionMsg(TCPSession::PTR session, const char* buffer, int len)
{
    cout << "on recv msg :" << string(buffer, len) << endl;
    //can session->send(buffer, len);
    return len;
}

int main()
{
    WrapServer::PTR server = std::make_shared<WrapServer>();

    server->setDefaultEnterCallback([](TCPSession::PTR session){
        cout << "on client enter" << endl;
        session->setCloseCallback(onSessionClose);
        session->setDataCallback(onSessionMsg);
        //can session->setUD(/*any*/);
    });

    //can server.addSession(fd, anyEnterCallback);

    server->startListen(8080);
    server->startWorkThread(2);

    std::cin.get();
}