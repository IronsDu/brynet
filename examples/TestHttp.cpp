#include <iostream>
#include <string>
#include <mutex>
#include <condition_variable>

#include "SocketLibFunction.h"
#include "HttpServer.h"
#include "HttpFormat.h"
#include "WebSocketFormat.h"

using namespace brynet;
using namespace brynet::net;

int main(int argc, char **argv)
{
    auto server = HttpServer::Create();

    server->startListen(false, "0.0.0.0", 8080);
    server->startWorkThread(ox_getcpunum());

    std::string body = "<html>hello world </html>";

    server->setEnterCallback([=](HttpSession::PTR& session){
        session->setHttpCallback([=](const HTTPParser& httpParser, HttpSession::PTR session){
            HttpResponse response;
            response.setBody(body);
            std::string result = response.getResult();
            session->send(result.c_str(), result.size(), std::make_shared<std::function<void(void)>>([session](){
                session->postShutdown();
            }));
        });
    });

    std::cin.get();

    sock fd = ox_socket_connect(false, "192.168.12.128", 8080);
    server->addConnection(fd, [](HttpSession::PTR session){
        HttpRequest request;
        HttpQueryParameter parameter;
        parameter.add("value", "123456");

        if (false)
        {
            request.addHeadValue("Accept", "*/*");
            request.setMethod(HttpRequest::HTTP_METHOD::HTTP_METHOD_PUT);
            request.setUrl("/v2/keys/asea/aagee");
            request.setContentType("application/x-www-form-urlencoded");
            request.setBody(parameter.getResult());
        }
        else
        {
            request.setMethod(HttpRequest::HTTP_METHOD::HTTP_METHOD_GET);
            request.setUrl("/v2/keys/asea/aagee");
            request.setQuery(parameter.getResult());
        }
        std::string requestStr = request.getResult();
        session->send(requestStr.c_str(), requestStr.size());

    }, [](const HTTPParser& httpParser, HttpSession::PTR session){
        //http response handle
        std::cout << httpParser.getBody() << std::endl;
    }, [](HttpSession::PTR session, WebSocketFormat::WebSocketFrameType, const std::string& payload){
        //websocket frame handle
    },[](HttpSession::PTR){
        //ws connected handle
    });

    std::cin.get();
}
