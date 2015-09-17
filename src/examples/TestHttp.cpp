#include <iostream>
#include <mutex>
#include <string>
#include <map>

#include "SocketLibFunction.h"
#include "HttpServer.h"

int main(int argc, char **argv)
{
    HttpServer server;

    server.start(80, 1);

    server.setRequestHandle([](const HTTPProtocol& httpProtocol, TCPSession::PTR session){
        /*处理request*/
        char response[1024];
        const char* body = "<htm>hello</html>";
        sprintf(response, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: %d\r\n\r\n%s", strlen(body), body);
        session->send(response, strlen(response));
    });

    sock fd = ox_socket_connect("180.87.33.107", 80);
    server.addConnection(fd, [](TCPSession::PTR session){
        /*发送http request*/
    }, [](const HTTPProtocol& httpProtocol, TCPSession::PTR session){
        /*处理response*/
    });
    std::cin.get();
}