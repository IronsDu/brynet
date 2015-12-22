#include <iostream>
#include <string>

#include "SocketLibFunction.h"
#include "HttpServer.h"
#include "HttpFormat.h"
#include "WebSocketFormat.h"

int main(int argc, char **argv)
{
    HttpServer server;

    server.start(8088, 1);

    server.setRequestHandle([](const HTTPParser& httpParser, TCPSession::PTR session, const char* websocketPacket, size_t websocketPacketLen){
        if (websocketPacket != nullptr)
        {
            std::string sendPayload = "hello";
            std::string sendFrame;
            WebSocketFormat::wsFrameBuild(sendPayload, sendFrame);

            session->send(sendFrame.c_str(), sendFrame.size());
        }
        else
        {
            //普通http协议
            HttpFormat httpFormat;
            httpFormat.setProtocol(HttpFormat::HRP_RESPONSE);
            httpFormat.addParameter("<html>hello</html>");
            std::string result = httpFormat.getResult();
            session->send(result.c_str(), result.size());
        }
    });

    sock fd = ox_socket_connect("180.87.33.107", 80);
    server.addConnection(fd, [](TCPSession::PTR session){

        /*发送http request*/

    }, [](const HTTPParser& httpParser, TCPSession::PTR session, const char* websocketPacket, size_t websocketPacketLen){
        /*处理response*/
    });

    std::cin.get();
}