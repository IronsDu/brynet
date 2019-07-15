#include <iostream>
#include <string>
#include <mutex>
#include <condition_variable>

#include <brynet/net/SSLHelper.h>
#include <brynet/net/SocketLibFunction.h>
#include <brynet/net/http/HttpService.h>
#include <brynet/net/http/HttpFormat.h>
#include <brynet/net/http/WebSocketFormat.h>
#include <brynet/net/Wrapper.h>

using namespace brynet;
using namespace brynet::net;
using namespace brynet::net::http;

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage: <listen port> <net work thread num>\n");
        exit(-1);
    }

    const auto port = std::atoi(argv[1]);
    auto service = TcpService::Create();
    service->startWorkerThread(atoi(argv[2]));

    auto httpEnterCallback = [](const HTTPParser& httpParser,
            const HttpSession::Ptr& session) {
                (void)httpParser;
                HttpResponse response;
                std::string body = "<html>hello world </html>";
                response.setBody(body);
                std::string result = response.getResult();
                session->send(result.c_str(), result.size(), [session]() {
                        session->postShutdown();
                    });
            };

    auto wsEnterCallback = [](const HttpSession::Ptr& httpSession,
            WebSocketFormat::WebSocketFrameType opcode,
            const std::string& payload) {
                std::cout << "frame enter of type:" << int(opcode) << std::endl;
                std::cout << "payload is:" << payload << std::endl;
                // echo frame
                auto frame = std::make_shared<std::string>();
                WebSocketFormat::wsFrameBuild(payload.c_str(),
                    payload.size(),
                    *frame,
                    WebSocketFormat::WebSocketFrameType::TEXT_FRAME,
                    true,
                    false);
                httpSession->send(frame);
            };

    wrapper::HttpListenerBuilder listenBuilder;
    listenBuilder.configureService(service)
        .configureSocketOptions({
                [](TcpSocket& socket) {
                    socket.setNodelay();
            },
        })
        .configureConnectionOptions({
            TcpService::AddSocketOption::WithMaxRecvBufferSize(1024),
        })
        .configureListen([port](wrapper::BuildListenConfig builder) {
            builder.setAddr(false, "0.0.0.0", port);
        })
        .configureEnterCallback([httpEnterCallback, wsEnterCallback](const HttpSession::Ptr& httpSession) {
            httpSession->setHttpCallback(httpEnterCallback);
            httpSession->setWSCallback(wsEnterCallback);
        })
        .asyncRun();

    std::cin.get();
    return 0;
}
