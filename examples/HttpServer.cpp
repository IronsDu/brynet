#include <brynet/base/AppStatus.hpp>
#include <brynet/net/http/HttpFormat.hpp>
#include <brynet/net/http/HttpService.hpp>
#include <brynet/net/http/WebSocketFormat.hpp>
#include <brynet/net/wrapper/HttpServiceBuilder.hpp>
#include <brynet/net/wrapper/ServiceBuilder.hpp>
#include <condition_variable>
#include <iostream>
#include <string>

using namespace brynet;
using namespace brynet::net;
using namespace brynet::net::http;

int main(int argc, char** argv)
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
        (void) httpParser;
        HttpResponse response;
        //std::cout << "method:" << http_method_str(static_cast<http_method>(httpParser.method())) << std::endl;
        response.setBody(std::string("<html>hello world </html>"));
        if (httpParser.isKeepAlive())
        {
            response.addHeadValue("Connection", "Keep-Alive");
            session->send(response.getResult());
        }
        else
        {
            response.addHeadValue("Connection", "Close");
            session->send(response.getResult(), [session]() {
                session->postShutdown();
            });
        }
    };

    auto wsEnterCallback = [](const HttpSession::Ptr& httpSession,
                              WebSocketFormat::WebSocketFrameType opcode,
                              const std::string& payload) {
        std::cout << "frame enter of type:" << int(opcode) << std::endl;
        std::cout << "payload is:" << payload << std::endl;
        // echo frame
        std::string frame;
        WebSocketFormat::wsFrameBuild(payload.c_str(),
                                      payload.size(),
                                      frame,
                                      WebSocketFormat::WebSocketFrameType::TEXT_FRAME,
                                      true,
                                      false);
        httpSession->send(std::move(frame));
    };

    wrapper::HttpListenerBuilder listenBuilder;
    listenBuilder
            .WithService(service)
            .AddSocketProcess([](TcpSocket& socket) {
                socket.setNodelay();
            })
            .WithMaxRecvBufferSize(1024)
            .WithAddr(false, "0.0.0.0", port)
            .WithReusePort()
            .WithEnterCallback([httpEnterCallback, wsEnterCallback](const HttpSession::Ptr& httpSession, HttpSessionHandlers& handlers) {
                handlers.setHttpCallback(httpEnterCallback);
                handlers.setWSCallback(wsEnterCallback);
            })
            .asyncRun();

    while (true)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (brynet::base::app_kbhit())
        {
            break;
        }
    }

    return 0;
}
