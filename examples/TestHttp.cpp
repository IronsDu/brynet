#include <iostream>
#include <string>
#include <mutex>
#include <condition_variable>

#include "SocketLibFunction.h"
#include "HttpServer.h"
#include "HttpFormat.h"
#include "WebSocketFormat.h"

using namespace dodo;
using namespace dodo::net;

int main(int argc, char **argv)
{
    HttpServer server;

    server.startListen(false, "0.0.0.0", 8080);
    server.startWorkThread(ox_getcpunum());
    std::string body;
    body += "<html>";
    for(int i = 0; i < 1; i ++)
    {
        body += "hello";
    }
    body += "</html>";
    server.setEnterCallback([&body](HttpSession::PTR& session){
        session->setHttpCallback([&body](const HTTPParser& httpParser, HttpSession::PTR session){
            //普通http协议
            HttpFormat httpFormat;
            httpFormat.setProtocol(HttpFormat::HTP_RESPONSE);
            httpFormat.addParameter(body.c_str());
            std::string result = httpFormat.getResult();
            session->send(result.c_str(), result.size(), std::make_shared<std::function<void(void)>>([session](){
                session->postShutdown();
            }));
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
        session->send(requestStr.c_str(), requestStr.size());

    }, [](const HTTPParser& httpParser, HttpSession::PTR session){
        std::cout << httpParser.getBody() << std::endl;
        return;
        /*处理response*/
    }, [](HttpSession::PTR session, WebSocketFormat::WebSocketFrameType, const std::string& payload){
    },[](HttpSession::PTR){
    });

    std::cin.get();
}
