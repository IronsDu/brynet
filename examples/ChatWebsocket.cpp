#include <iostream>
#include <string>
#include <condition_variable>
#include <mutex>
#include <list>
#include <regex>

#include <brynet/net/http/HttpService.hpp>
#include <brynet/net/http/HttpFormat.hpp>
#include <brynet/net/http/WebSocketFormat.hpp>
#include <brynet/net/wrapper/ServiceBuilder.hpp>
#include <brynet/net/wrapper/HttpServiceBuilder.hpp>
#include <brynet/base/AppStatus.hpp>

using namespace brynet;
using namespace brynet::net;
using namespace brynet::net::http;

std::string getBody()
{

    return R""(<!DOCTYPE html>
               <html>
                   <head>
                       <meta content="text/html;charset=utf-8" http-equiv="Content-Type">
                       <meta content="utf-8" http-equiv="encoding">
                   </head>
                   <body>
                       <p>Message received: <span id="msgrec"></span></p>
                       <p>Message to send: <input id="mymsg"></span></p>
                       <button type="button" onclick="sendMessage()">Send Message</button>
                   </body>
                   <script>
                   var ws = new WebSocket("ws://localhost:_PORT_");
                   var msgrec = document.getElementById("msgrec")
                   var mymsg = document.getElementById("mymsg")

                   ws.onmessage = function (evt) {
                       msgrec.innerHTML = evt.data;
                   };

                   function sendMessage() {
                       if (ws)
                           ws.send(mymsg.value)
                   }
                   </script>
               </html>)"";
}


int main(int argc, char **argv)
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage: <listen port> <net work thread num>\n");
        exit(-1);
    }

    std::mutex lockClients;
    std::list<HttpSession::Ptr> clients;

    const auto port = std::atoi(argv[1]);
    auto service = TcpService::Create();
    service->startWorkerThread(atoi(argv[2]));

    auto httpEnterCallback = [&port](const HTTPParser& httpParser,
            const HttpSession::Ptr& session) {
                (void)httpParser;
                HttpResponse response;
                auto body = std::regex_replace(getBody(), std::regex("_PORT_"), std::to_string(port));

                response.setBody(body);
                std::string result = response.getResult();
                session->send(result.c_str(), result.size(), [session]() {
                        session->postShutdown();
                    });
            };

    auto wsEnterCallback = [&clients, &lockClients](const HttpSession::Ptr& httpSession,
            WebSocketFormat::WebSocketFrameType opcode,
            const std::string& payload) {
                std::cout << "frame enter of type:" << int(opcode) << std::endl;
                std::cout << "payload is:" << payload << std::endl;

                switch (opcode) {
                case WebSocketFormat::WebSocketFrameType::CLOSE_FRAME: 
                    clients.remove(httpSession);
                    break;
                case WebSocketFormat::WebSocketFrameType::TEXT_FRAME:
                    // send back the message to each connected client
                    for (auto client : clients) {
                        lockClients.lock();
                        auto frame = std::make_shared<std::string>();
                        WebSocketFormat::wsFrameBuild(payload.c_str(),
                            payload.size(),
                            *frame,
                            WebSocketFormat::WebSocketFrameType::TEXT_FRAME,
                            true,
                            false);
                        client->send(frame);
                        lockClients.unlock();
                    }
                    break;
                case WebSocketFormat::WebSocketFrameType::ERROR_FRAME:
                case WebSocketFormat::WebSocketFrameType::CONTINUATION_FRAME:
                case WebSocketFormat::WebSocketFrameType::BINARY_FRAME:
                case WebSocketFormat::WebSocketFrameType::PING_FRAME:
                case WebSocketFormat::WebSocketFrameType::PONG_FRAME:
                    break;
                }
            };

    auto wsConnectedCallback = [&clients, &lockClients](const HttpSession::Ptr& httpSession,
            const HTTPParser&) {
                lockClients.lock();
                clients.push_back(httpSession);
                lockClients.unlock();
            };

    auto closedCallback = [&clients, &lockClients](const HttpSession::Ptr& httpSession) {
                clients.remove(httpSession);
            };

    wrapper::HttpListenerBuilder listenBuilder;
    listenBuilder.configureService(service)
        .configureSocketOptions({
                [](TcpSocket& socket) {
                    socket.setNodelay();
            },
        })
        .configureConnectionOptions({
            AddSocketOption::WithMaxRecvBufferSize(1024),
        })
        .configureListen([port](wrapper::BuildListenConfig builder) {
            builder.setAddr(false, "0.0.0.0", port);
        })
        .configureEnterCallback([httpEnterCallback, wsEnterCallback, wsConnectedCallback, closedCallback]
         (const HttpSession::Ptr& httpSession, HttpSessionHandlers& handlers) {
            (void)httpSession;
            handlers.setHttpCallback(httpEnterCallback);
            handlers.setWSCallback(wsEnterCallback);
            handlers.setWSConnected(wsConnectedCallback);
            handlers.setClosedCallback(closedCallback);
        })
        .asyncRun();

    while(true)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (brynet::base::app_kbhit())
        {
            break;
        }
    }

    return 0;
}
