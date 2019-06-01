#include <iostream>
#include <string>
#include <mutex>
#include <condition_variable>
#include <thread>

#include <brynet/net/SocketLibFunction.h>
#include <brynet/net/http/HttpService.h>
#include <brynet/net/http/HttpFormat.h>
#include <brynet/net/http/WebSocketFormat.h>
#include <brynet/utils/packet.h>
#include <brynet/net/Wrapper.h>

using namespace brynet;
using namespace brynet::net;
using namespace brynet::net::http;
using namespace brynet::utils;

std::atomic<int32_t> count;

static void sendPacket(HttpSession::Ptr session, const char* data, size_t len)
{
    char buff[1024];
    BasePacketWriter bw(buff, sizeof(buff), true, true);
    bw.writeINT8('{');
    bw.writeBuffer("\"data\":\"", 8);
    bw.writeBuffer(data, len);
    bw.writeINT8('"');
    bw.writeINT8('}');

    auto frame = std::make_shared<std::string>();
    WebSocketFormat::wsFrameBuild(bw.getData(),
        bw.getPos(), 
        *frame, 
        WebSocketFormat::WebSocketFrameType::TEXT_FRAME, 
        true, 
        true);
    session->send(frame);
}

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        std::cout << "./benchwebsocket host port [ connections workers ]";
        return 1;
    }
    
    const char* host = argv[1];
    int port = std::atoi(argv[2]);
    int connections = argc > 3 ? std::atoi(argv[3]) : 200;
    size_t workers = argc > 4 ? std::atoi(argv[4]) : std::thread::hardware_concurrency();
    
    std::cout << "host: " << host << ':' << port << " | connections: " << connections << " | workers: " << workers << std::endl;

    auto enterCallback = [host](const HttpSession::Ptr& httpSession) {
        HttpRequest request;
        request.setMethod(HttpRequest::HTTP_METHOD::HTTP_METHOD_GET);
        request.setUrl("/ws");
        request.addHeadValue("Host", host);
        request.addHeadValue("Upgrade", "websocket");
        request.addHeadValue("Connection", "Upgrade");
        request.addHeadValue("Sec-WebSocket-Key", "dGhlIHNhbXBsZSBub25jZQ==");
        request.addHeadValue("Sec-WebSocket-Version", "13");

        std::string requestStr = request.getResult();
        httpSession->send(requestStr.c_str(), requestStr.size());

        httpSession->setWSConnected([](const HttpSession::Ptr& session, const HTTPParser&) {
                for (int i = 0; i < 200; i++)
                {
                    sendPacket(session, "hello, world!", 13);
                }
            });

        httpSession->setWSCallback([](const HttpSession::Ptr& session,
            WebSocketFormat::WebSocketFrameType, const std::string& payload) {
                std::cout << payload << std::endl;
                sendPacket(session, "hello, world!", 13);
                count += 1;
            });
    };

    auto service = TcpService::Create();
    service->startWorkerThread(workers);

    auto connector = AsyncConnector::Create();
    connector->startWorkerThread();

    wrapper::HttpConnectionBuilder connectionBuilder;
    connectionBuilder.configureService(service)
        .configureConnector(connector)
        .configureConnectionOptions({
            brynet::net::TcpService::AddSocketOption::WithMaxRecvBufferSize(1024 * 1024)
        });

    for (int i = 0; i < connections; i++)
    {
        connectionBuilder.configureConnectOptions({
                AsyncConnector::ConnectOptions::WithAddr(host, port),
                AsyncConnector::ConnectOptions::WithTimeout(std::chrono::seconds(10)),
                AsyncConnector::ConnectOptions::AddProcessTcpSocketCallback([](TcpSocket& socket) {
                    socket.setNodelay();
                })
            })
            .configureEnterCallback(enterCallback)
            .asyncConnect();
    }

    brynet::net::EventLoop mainLoop;
    while (true)
    {
        mainLoop.loop(5000);
        std::cout << (count / 5) << std::endl;
        count = 0;
    }
    std::cin.get();
}
