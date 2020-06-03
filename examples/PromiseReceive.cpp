#include <iostream>
#include <mutex>
#include <brynet/net/EventLoop.hpp>
#include <brynet/net/TcpService.hpp>
#include <brynet/net/PromiseReceive.hpp>
#include <brynet/net/http/HttpFormat.hpp>
#include <brynet/net/wrapper/ServiceBuilder.hpp>
#include <brynet/base/AppStatus.hpp>

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

    auto service = TcpService::Create();
    service->startWorkerThread(atoi(argv[2]));

    auto enterCallback = [](const TcpConnection::Ptr& session) {
        auto promiseReceive = setupPromiseReceive(session);
        auto contentLength = std::make_shared<size_t>();

        promiseReceive
            ->receiveUntil("\r\n", [](const char* buffer, size_t len) {
                auto headline = std::string(buffer, len);
                std::cout << headline << std::endl;
                return false;
            })
            ->receiveUntil("\r\n", [promiseReceive, contentLength](const char* buffer, size_t len) {
                auto headerValue = std::string(buffer, len);
                std::cout << headerValue << std::endl;
                if (len > 2)
                {
                    const static std::string ContentLenghtFlag = "Content-Length: ";
                    auto pos = headerValue.find(ContentLenghtFlag);
                    if (pos != std::string::npos)
                    {
                        auto lenStr = headerValue.substr(pos + ContentLenghtFlag.size(), headerValue.size());
                        *contentLength = std::stoi(lenStr);
                    }
                    return true;
                }
                return false;
            })->receive(contentLength, [session](const char* buffer, size_t len) {
                (void)buffer;
                (void)len;
                HttpResponse response;
                response.setStatus(HttpResponse::HTTP_RESPONSE_STATUS::OK);
                response.setContentType("text/html; charset=utf-8");
                response.setBody("<html>hello world </html>");

                auto result = response.getResult();
                session->send(result.c_str(), result.size());
                session->postShutdown();

                return false;
            });
    };

    wrapper::ListenerBuilder listener;
    listener.configureService(service)
        .configureSocketOptions({
            [](TcpSocket& socket) {
                socket.setNodelay();
            }
        })
        .configureConnectionOptions({
            brynet::net::AddSocketOption::WithMaxRecvBufferSize(1024 * 1024),
            brynet::net::AddSocketOption::AddEnterCallback(enterCallback)
        })
        .configureListen([=](wrapper::BuildListenConfig config) {
            config.setAddr(false, "0.0.0.0", atoi(argv[1]));
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
