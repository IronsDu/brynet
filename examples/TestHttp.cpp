#include <iostream>
#include <string>
#include <mutex>
#include <condition_variable>

#include "SocketLibFunction.h"
#include "HttpServer.h"
#include "HttpFormat.h"
#include "WebSocketFormat.h"

int main(int argc, char **argv)
{
    HttpServer server;

    server.startListen(false, "127.0.0.1", 8080);
    server.startWorkThread(ox_getcpunum());
    server.setEnterCallback([](HttpSession::PTR session){
        session->setRequestCallback([](const HTTPParser& httpParser, HttpSession::PTR session, const char* websocketPacket, size_t websocketPacketLen){
            if (websocketPacket != nullptr)
            {
                std::string sendPayload = "hello";
                std::string sendFrame;
                WebSocketFormat::wsFrameBuild(sendPayload, sendFrame);

                session->getSession()->send(sendFrame.c_str(), sendFrame.size());
            }
            else
            {
                //普通http协议
                HttpFormat httpFormat;
                httpFormat.setProtocol(HttpFormat::HTP_RESPONSE);
                httpFormat.addParameter("<html>hello</html>");
                std::string result = httpFormat.getResult();
                session->getSession()->send(result.c_str(), result.size(), std::make_shared<std::function<void(void)>>([session](){
                    session->getSession()->postShutdown();
                }));
            }
        });
    });

    std::cin.get();

    sock fd = ox_socket_connect(false, "192.168.12.128", 8080);
    server.addConnection(fd, [](HttpSession::PTR session){
        HttpFormat request;
        if (false)
        {
            request.addHeadValue("Accept", "*/*");
            request.setProtocol(HttpFormat::HTP_PUT);
            request.setRequestUrl("/v2/keys/asea/aagee");
            request.addParameter("value", "123456");
            request.setContentType("application/x-www-form-urlencoded");
        }
        else
        {
            request.setProtocol(HttpFormat::HTP_GET);
            request.setRequestUrl("/v2/keys/asea/aagee");
            request.addParameter("value", "123456");
        }
        std::string requestStr = request.getResult();
        session->getSession()->send(requestStr.c_str(), requestStr.size());

    }, [](const HTTPParser& httpParser, HttpSession::PTR session, const char* websocketPacket, size_t websocketPacketLen){
        std::cout << httpParser.getBody() << std::endl;
        return;
        /*处理response*/
    }, [](HttpSession::PTR){
    });

    std::cin.get();
}
