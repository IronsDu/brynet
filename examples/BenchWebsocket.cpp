#include <iostream>
#include <string>
#include <mutex>
#include <condition_variable>

#include <brynet/net/SocketLibFunction.h>
#include <brynet/net/http/HttpService.h>
#include <brynet/net/http/HttpFormat.h>
#include <brynet/net/http/WebSocketFormat.h>
#include <brynet/utils/packet.h>
#include <brynet/utils/systemlib.h>

using namespace brynet;
using namespace brynet::net;

std::atomic<int32_t> count;

static void sendPacket(HttpSession::PTR session, const char* data, size_t len)
{
    char buff[1024];
    BasePacketWriter bw(buff, sizeof(buff), true, true);
    bw.writeINT8('{');
    bw.writeINT32(len + 14);
    bw.writeINT32(10000);
    bw.writeINT32(0);
    bw.writeBuffer(data, len);
    bw.writeINT8('}');

    auto frame = std::make_shared<std::string>();
    WebSocketFormat::wsFrameBuild(bw.getData(), bw.getPos(), *frame, WebSocketFormat::WebSocketFrameType::TEXT_FRAME, true, true);
    session->send(frame);
}

int main(int argc, char **argv)
{
    auto service = std::make_shared<WrapTcpService>();
    service->startWorkThread(ox_getcpunum());

    for (int i = 0; i < 200; i++)
    {
        sock fd = ox_socket_connect(false, "192.168.2.78", 8008);
        ox_socket_nodelay(fd);
        service->addSession(fd, [](const TCPSession::PTR& session) {
            HttpService::setup(session, [](const HttpSession::PTR& httpSession) {
                HttpRequest request;
                request.setMethod(HttpRequest::HTTP_METHOD::HTTP_METHOD_GET);
                request.setUrl("/ws");
                request.addHeadValue("Host", "192.168.2.78");
                request.addHeadValue("Upgrade", "websocket");
                request.addHeadValue("Connection", "Upgrade");
                request.addHeadValue("Sec-WebSocket-Key", "dGhlIHNhbXBsZSBub25jZQ==");
                request.addHeadValue("Sec-WebSocket-Version", "13");

                std::string requestStr = request.getResult();
                httpSession->send(requestStr.c_str(), requestStr.size());

                httpSession->setWSConnected([](const HttpSession::PTR& session, const HTTPParser&) {
                    for (int i = 0; i < 200; i++)
                    {
                        sendPacket(session, "hello world", 10);
                    }
                });

                httpSession->setWSCallback([](const HttpSession::PTR& session,
                    WebSocketFormat::WebSocketFrameType, const std::string& payload) {
                    sendPacket(session, "hello world", 10);
                    count += 1;
                });
            });
        }, false, nullptr, 1024 * 1024, false);
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
