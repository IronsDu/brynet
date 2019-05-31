#include <iostream>
#include <string>
#include <mutex>
#include <condition_variable>

#include <brynet/net/SSLHelper.h>
#include <brynet/net/SocketLibFunction.h>
#include <brynet/net/http/HttpService.h>
#include <brynet/net/http/HttpFormat.h>
#include <brynet/net/http/WebSocketFormat.h>
#include <brynet/net/Connector.h>
#include <brynet/net/Wrapper.h>

using namespace brynet;
using namespace brynet::net;
using namespace brynet::net::http;

void reqHttp(TcpService::Ptr service, 
    AsyncConnector::Ptr connector,
    std::string requestStr,
    std::vector<AsyncConnector::ConnectOptions::ConnectOptionFunc> options,
    std::vector<TcpService::AddSocketOption::AddSocketOptionFunc> sessionOptions,
    std::function<void(const HTTPParser& httpParser, const HttpSession::Ptr& session)> userCallback)
{
    auto enterCallback = [=](TcpSocket::Ptr socket) mutable {
        socket->setNodelay();

        auto enterCallback = [=](const TcpConnection::Ptr& session) {
            HttpService::setup(session, [=](const HttpSession::Ptr& httpSession) {
                httpSession->send(requestStr.c_str(), requestStr.size());
                httpSession->setHttpCallback([=](const HTTPParser& httpParser, const HttpSession::Ptr& session) {
                    userCallback(httpParser, session);
                });
            });
        };

        sessionOptions.push_back(brynet::net::TcpService::AddSocketOption::AddEnterCallback(enterCallback));
        service->addTcpConnection(std::move(socket), sessionOptions);
    };

    options.push_back(AsyncConnector::ConnectOptions::WithCompletedCallback(enterCallback));
    connector->asyncConnect(options);
};


int main(int argc, char **argv)
{
    auto service = TcpService::Create();
    service->startWorkerThread(2);

    auto connector = brynet::net::AsyncConnector::Create();
    connector->startWorkerThread();

    auto listenThread = ListenThread::Create(false, "0.0.0.0", 8080, [service](TcpSocket::Ptr socket) {
        std::string body = "<html>hello world </html>";
        auto enterCallback = [body](const TcpConnection::Ptr& session) {
            HttpService::setup(session, [body](const HttpSession::Ptr& httpSession) {
                httpSession->setHttpCallback([body](const HTTPParser& httpParser,
                    const HttpSession::Ptr& session) {
                        HttpResponse response;
                        response.setBody(body);
                        std::string result = response.getResult();
                        session->send(result.c_str(), result.size(), [session]() {
                            session->postShutdown();
                            });
                    });

                httpSession->setWSCallback([](const HttpSession::Ptr& httpSession,
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
        service->addTcpConnection(std::move(socket),
            brynet::net::TcpService::AddSocketOption::AddEnterCallback(enterCallback),
            brynet::net::TcpService::AddSocketOption::WithMaxRecvBufferSize(10));
        });
    listenThread->startListen();

    wrapper::SocketConnectBuilder sb;
    auto s = sb.configureConnector(connector)
        .configureConnectOptions({
            AsyncConnector::ConnectOptions::WithTimeout(std::chrono::seconds(2)),
            AsyncConnector::ConnectOptions::WithAddr("127.0.0.1", 8010)
            })
        .syncConnect();
    s = sb.configureConnector(connector)
        .configureConnectOptions({
            AsyncConnector::ConnectOptions::WithAddr("127.0.0.1", 8080)
            })
        .syncConnect();

    {
        wrapper::ConnectionBuilder sb;
        auto s = sb.configureConnector(connector)
            .configureConnectOptions({
                AsyncConnector::ConnectOptions::WithTimeout(std::chrono::seconds(2)),
                AsyncConnector::ConnectOptions::WithAddr("127.0.0.1", 8010)
                })
            .syncConnect();
    }

    HttpRequest request;
    request.setMethod(HttpRequest::HTTP_METHOD::HTTP_METHOD_GET);
    request.setUrl("/ISteamUserAuth/AuthenticateUserTicket/v1/");
    request.addHeadValue("Host", "api.steampowered.com");

    HttpQueryParameter p;
    p.add("key", "DCD9C36F1F54A96F707DFBE833600167");
    p.add("appid", "929390");
    p.add("ticket", "140000006FC57764C95D45085373F10401001001359F745C1800000001000000020000009DACD3DE1202A8C0431E100003000000B200000032000000040000005373F104010010016E2E0E009DACD3DE1202A8C000000000AAA16F5C2A518B5C0100FC96040000000000061129B849B0397DD62E0B1B0373451EC08E1BAB70FC18E21094FC5F4674EDD50226ABB33D71C601B8E65542FB9A9F48BFF87AC30904D272FAD5F15CD2D5428D44827BA58A45886119D6244D672A0C1909C5D7BD9096D96EB8BAC30E006BE6D405E5B25659CF3D343C9627078C5FD4CE0120D80DDB2FA09E76111143F132CA0B");
    request.setQuery(p.getResult());

    std::string requestStr = request.getResult();

    std::atomic<int> couner{ 0 };

    for (size_t i = 0; i < 10; i++)
    {
        reqHttp(service,
            connector,
            requestStr,
            {
                AsyncConnector::ConnectOptions::WithAddr("23.73.140.64", 80),
                AsyncConnector::ConnectOptions::WithTimeout(std::chrono::seconds(10)),
            },
            {
            },
            [&](const HTTPParser& httpParser, const HttpSession::Ptr& session) {
                std::cout << ++couner << std::endl;
            });
    }

    wrapper::HttpConnectionBuilder()
        .configureConnector(connector)
        .configureService(service)
        .configureConnectOptions({
            AsyncConnector::ConnectOptions::WithAddr("180.97.33.108", 80),
            AsyncConnector::ConnectOptions::WithTimeout(std::chrono::seconds(10)),
            AsyncConnector::ConnectOptions::WithFailedCallback([]() {
                    std::cout << "failed" << std::endl;
                }),
        })
        .configureConnectionOptions({
            TcpService::AddSocketOption::WithMaxRecvBufferSize(1024),
            TcpService::AddSocketOption::AddEnterCallback([](const TcpConnection::Ptr& session) {
                // do something for session
                std::cout << "success" << std::endl;
            })
        })
        .configureEnterCallback([](HttpSession::Ptr session) {
            std::cout << "success" << std::endl;
        })
        .asyncConnect();

    wrapper::ListenerBuilder listenBuilder;
    listenBuilder.configureService(service)
        .configureSocketOptions({
                [](TcpSocket& socket) {
                    socket.setNodelay();
            },
        })
        .configureConnectionOptions({
            TcpService::AddSocketOption::WithMaxRecvBufferSize(1024),
        })
        .configureListen([](wrapper::BuildListenConfig builder) {
            builder.setAddr(false, "0.0.0.0", 80);
        })
        .asyncRun();

    std::cin.get();
    return 0;
}
