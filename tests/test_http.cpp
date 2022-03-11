#define CATCH_CONFIG_MAIN// This tells Catch to provide a main() - only do this in one cpp file
#include <brynet/net/ListenThread.hpp>
#include <brynet/net/wrapper/ConnectionBuilder.hpp>
#include <brynet/net/wrapper/ServiceBuilder.hpp>
#include <brynet/net/wrapper/HttpConnectionBuilder.hpp>
#include <brynet/net/wrapper/HttpServiceBuilder.hpp>
#include <brynet/net/http/HttpFormat.hpp>
#include <brynet/base/WaitGroup.hpp>
#include <cstdlib>
#include <ctime>

#include "catch.hpp"

TEST_CASE("http server are computed", "[http_server]")
{
    using namespace brynet;
    using namespace brynet::net;
    using namespace brynet::net::http;

    const std::string ip = "127.0.0.1";
    const auto port = 9999;

    std::srand(std::time(nullptr));
    // 开启监听服务
    {
        {
            static std::atomic_llong counter = ATOMIC_VAR_INIT(0);
            static std::atomic_bool useShutdown = ATOMIC_VAR_INIT(false);

            auto connector = AsyncConnector::Create();
            connector->startWorkerThread();

            auto service = TcpService::Create();
            service->startWorkerThread(1);

            auto httpEnterCallback = [](const HTTPParser& httpParser,
                const HttpSession::Ptr& session) {
                    REQUIRE(httpParser.getPath() == "/ISteamUserAuth/AuthenticateUserTicket/v1/");
                    REQUIRE(httpParser.getQuery() == "key=DCD9C36F1F54A96F707DFBE833600167&appid=929390&ticket=140000006FC57764C95D45085373F10401001001359F745C1800000001000000020000009DACD3DE1202A8C0431E100003000000B200000032000000040000005373F104010010016E2E0E009DACD3DE1202A8C000000000AAA16F5C2A518B5C0100FC96040000000000061129B849B0397DD62E0B1B0373451EC08E1BAB70FC18E21094FC5F4674EDD50226ABB33D71C601B8E65542FB9A9F48BFF87AC30904D272FAD5F15CD2D5428D44827BA58A45886119D6244D672A0C1909C5D7BD9096D96EB8BAC30E006BE6D405E5B25659CF3D343C9627078C5FD4CE0120D80DDB2FA09E76111143F132CA0B");
                    REQUIRE(httpParser.method() == static_cast<int>(HttpRequest::HTTP_METHOD::HTTP_METHOD_GET));
                    (void)httpParser;
                    HttpResponse response;
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
                                if (std::rand() / 2 == 0)
                                {
                                    useShutdown.store(true);
                                    session->postShutdown();
                                }
                                else
                                {
                                    session->postClose();
                                }
                            });
                    }
            };

            auto wsEnterCallback = [](const HttpSession::Ptr& httpSession,
                WebSocketFormat::WebSocketFrameType opcode,
                const std::string& payload) {
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

            brynet::base::WaitGroup::Ptr wg = brynet::base::WaitGroup::Create();
            wg->add();

            HttpRequest request;
            request.setMethod(HttpRequest::HTTP_METHOD::HTTP_METHOD_GET);
            request.setUrl("/ISteamUserAuth/AuthenticateUserTicket/v1/");
            request.addHeadValue("Host", "api.steampowered.com");

            HttpQueryParameter p;
            p.add("key", "DCD9C36F1F54A96F707DFBE833600167");
            p.add("appid", "929390");
            p.add("ticket",
                "140000006FC57764C95D45085373F104"
                "01001001359F745C1800000001000000020000009"
                "DACD3DE1202A8C0431E100003000000B200000032"
                "000000040000005373F104010010016E2E0E009D"
                "ACD3DE1202A8C000000000AAA16F5C2A518B5C"
                "0100FC96040000000000061129B849B0397DD"
                "62E0B1B0373451EC08E1BAB70FC18E21094F"
                "C5F4674EDD50226ABB33D71C601B8E65542F"
                "B9A9F48BFF87AC30904D272FAD5F15CD2D5428"
                "D44827BA58A45886119D6244D672A0C1909C5D"
                "7BD9096D96EB8BAC30E006BE6D405E5B25659"
                "CF3D343C9627078C5FD4CE0120D80DDB2FA09E76111143F132CA0B");
            request.setQuery(p.getResult());

            std::string requestStr = request.getResult();

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

                wrapper::HttpConnectionBuilder()
                    .WithConnector(connector)
                    .WithService(service)
                    .WithAddr("127.0.0.1", port)
                    .WithTimeout(std::chrono::seconds(10))
                    .WithFailedCallback([]() {
                        })
                    .WithMaxRecvBufferSize(10)
                            .WithEnterCallback([requestStr, wg](const HttpSession::Ptr& session, HttpSessionHandlers& handlers) {
                            (void)session;
                            session->send(requestStr);
                            handlers.setClosedCallback([wg](const HttpSession::Ptr& session) {
                                wg->done();
                                });
                            handlers.setHttpCallback([wg](const HTTPParser& httpParser,
                                const HttpSession::Ptr& session) {
                                    (void)session;
                                    REQUIRE(httpParser.getBody() == "<html>hello world </html>");
                                    counter.fetch_add(1);
                                    session->postClose();
                                });
                                })
                            .asyncConnect();
                wg->wait();
                if (useShutdown.load())
                {
                    REQUIRE(counter.load() == 1);
                }
        }
    }

}
