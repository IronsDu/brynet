#include <iostream>
#include <string>
#include <mutex>
#include <condition_variable>

#include <brynet/net/SocketLibFunction.h>
#include <brynet/net/http/HttpService.h>
#include <brynet/net/http/HttpFormat.h>
#include <brynet/net/http/WebSocketFormat.h>

using namespace brynet;
using namespace brynet::net;

int main(int argc, char **argv)
{
    std::string body = "<html>hello world </html>";

    auto service = std::make_shared<WrapTcpService>();
    service->startWorkThread(2);

    auto listenThread = ListenThread::Create();
    listenThread->startListen(false, "0.0.0.0", 8080, [service, body](TcpSocket::PTR socket) {
        auto enterCallback = [body](const TCPSession::PTR& session) {
            HttpService::setup(session, [body](const HttpSession::PTR& httpSession) {
                httpSession->setHttpCallback([body](const HTTPParser& httpParser,
                    const HttpSession::PTR& session) {
                    HttpResponse response;
                    response.setBody(body);
                    std::string result = response.getResult();
                    session->send(result.c_str(), result.size(), [session]() {
                        session->postShutdown();
                    });
                });

                httpSession->setWSCallback([](const HttpSession::PTR& httpSession,
                    WebSocketFormat::WebSocketFrameType opcode,
                    const std::string& payload) {
                    // ping pong
                    auto frame = std::make_shared<std::string>();
                    WebSocketFormat::wsFrameBuild(payload.c_str(),
                        payload.size(),
                        *frame,
                        WebSocketFormat::WebSocketFrameType::TEXT_FRAME,
                        true,
                        true);
                    httpSession->send(frame);
                });
            });
        };
        service->addSession(std::move(socket), 
            AddSessionOption::WithEnterCallback(enterCallback),
            AddSessionOption::WithMaxRecvBufferSize(1024 * 1024));
    });

#ifdef USE_OPENSSL
    sock fd = brynet::net::base::Connect(false, "180.97.33.108", 443);
    if (fd != SOCKET_ERROR)
    {
        auto socket = TcpSocket::Create(fd, false);
        SSL_library_init();
        auto enterCallback = [](const TCPSession::PTR& session) {
            HttpService::setup(session, [](const HttpSession::PTR& httpSession) {
                HttpRequest request;
                request.setMethod(HttpRequest::HTTP_METHOD::HTTP_METHOD_GET);
                request.setUrl("/");

                std::string requestStr = request.getResult();
                httpSession->send(requestStr.c_str(), requestStr.size());
                httpSession->setHttpCallback([](const HTTPParser& httpParser, const HttpSession::PTR& session) {
                    //http response handle
                    std::cout << httpParser.getBody() << std::endl;
                });
            });
        };

        service->addSession(std::move(socket), 
            AddSessionOption::WithClientSideSSL(),
            AddSessionOption::WithMaxRecvBufferSize(1024 * 1024));
    }
#endif

    std::cin.get();
    return 0;
}
