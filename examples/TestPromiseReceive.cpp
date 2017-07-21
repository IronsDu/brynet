#include <iostream>
#include <mutex>
#include <atomic>

#include "SocketLibFunction.h"
#include "EventLoop.h"
#include "WrapTCPService.h"
#include "PromiseReceive.h"
#include "HttpFormat.h"

using namespace brynet;
using namespace brynet::net;

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage: <listen port> <net work thread num>\n");
        exit(-1);
    }

    auto server = std::make_shared<WrapTcpService>();
    auto listenThread = ListenThread::Create();

    listenThread->startListen(false, "0.0.0.0", atoi(argv[1]), nullptr, nullptr, [=](int fd){
        server->addSession(fd, [](const TCPSession::PTR& session){

            auto promiseReceive = setupPromiseReceive(session);
            auto contentLength = std::make_shared<size_t>();

            promiseReceive->receiveUntil("\r\n", [](const char* buffer, size_t len) {
                std::cout << std::string(buffer, len);
                return false;
            })->receiveUntil("\r\n", [promiseReceive, contentLength](const char* buffer, size_t len) {
                std::cout << std::string(buffer, len);
                if (len > 2)
                {
                    // ¿ÉÒÔ¶ÁÈ¡Content-Length
                    return true;
                }
                return false;
            })->receive(contentLength, [session](const char* buffer, size_t len) {
                HttpResponse response;
                response.setStatus(HttpResponse::HTTP_RESPONSE_STATUS::OK);
                response.setContentType("text/html; charset=utf-8");
                response.setBody("<html>hello world </html>");
                
                auto result = response.getResult();
                session->send(result.c_str(), result.size());
                session->postShutdown();

                return false;
            });
        }, false, 1024*1024);
    });

    server->startWorkThread(atoi(argv[2]));

    EventLoop mainLoop;
    while (true)
    {
        mainLoop.loop(1000);
    }
}
