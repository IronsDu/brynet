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

    sock fd = ox_socket_connect("127.0.0.1", 2379);
    server.addConnection(fd, [](TCPSession::PTR session){
        HttpFormat request;
        request.addHeadValue("Accept", "*/*");
        request.setProtocol(HttpFormat::HRP_PUT);
        request.setRequestUrl("/v2/keys/asea/aagee");
        request.addParameter("value", "123456");
        request.setContentType("application/x-www-form-urlencoded");
        string requestStr = request.getResult();
        session->send(requestStr.c_str(), requestStr.size());

    }, [](const HTTPParser& httpParser, TCPSession::PTR session, const char* websocketPacket, size_t websocketPacketLen){
        return;
        /*处理response*/
    });

    std::cin.get();
}