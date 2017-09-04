#include <iostream>
#include <string>
#include <mutex>
#include <condition_variable>

#include <brynet/net/SocketLibFunction.h>
#include <brynet/net/http/HttpService.h>
#include <brynet/net/http/HttpFormat.h>
#include <brynet/net/http/WebSocketFormat.h>
#include <brynet/utils/systemlib.h>

using namespace brynet;
using namespace brynet::net;

int main(int argc, char **argv)
{
    std::string body = "<html>hello world </html>";

    auto service = std::make_shared<WrapTcpService>();
    service->startWorkThread(2);

    auto listenThread = ListenThread::Create();
    listenThread->startListen(false, "0.0.0.0", 8080, [service, body](sock fd) {
        service->addSession(fd, [body](const TCPSession::PTR& session) {
            HttpService::setup(session, [body](const HttpSession::PTR& httpSession) {
                httpSession->setHttpCallback([body](const HTTPParser& httpParser, const HttpSession::PTR& session) {
                    HttpResponse response;
                    response.setBody(body);
                    std::string result = response.getResult();
                    session->send(result.c_str(), result.size(), std::make_shared<std::function<void(void)>>([session]() {
                        session->postShutdown();
                    }));
                });

                httpSession->setWSCallback([](const HttpSession::PTR& httpSession, WebSocketFormat::WebSocketFrameType opcode, const std::string& payload) {
                    // ping pong
                    auto frame = std::make_shared<std::string>();
                    WebSocketFormat::wsFrameBuild(payload.c_str(), payload.size(), *frame, WebSocketFormat::WebSocketFrameType::TEXT_FRAME, true, true);
                    httpSession->send(frame);
                });
            });
        }, false, nullptr, 1024 * 1024, false);
    });

    std::cin.get();

    sock fd = ox_socket_connect(false, "192.168.12.128", 8080);
    if (fd != SOCKET_ERROR)
    {
        service->addSession(fd, [](const TCPSession::PTR& session) {
            HttpService::setup(session, [](const HttpSession::PTR& httpSession) {
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
                httpSession->send(requestStr.c_str(), requestStr.size());
                httpSession->setHttpCallback([](const HTTPParser& httpParser, const HttpSession::PTR& session) {
                    //http response handle
                    std::cout << httpParser.getBody() << std::endl;
                });
            });
        }, false, nullptr, 1024 * 1024, false);
    }

    std::cin.get();
    return 0;
}
