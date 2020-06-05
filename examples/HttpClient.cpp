#include <iostream>
#include <string>
#include <mutex>
#include <brynet/net/http/HttpService.hpp>
#include <brynet/net/http/HttpFormat.hpp>
#include <brynet/net/AsyncConnector.hpp>
#include <brynet/net/wrapper/HttpConnectionBuilder.hpp>
#include <brynet/base/AppStatus.hpp>

using namespace brynet;
using namespace brynet::net;
using namespace brynet::net::http;

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    auto service = TcpService::Create();
    service->startWorkerThread(2);

    auto connector = brynet::net::AsyncConnector::Create();
    connector->startWorkerThread();

    HttpRequest request;
    request.setMethod(HttpRequest::HTTP_METHOD::HTTP_METHOD_GET);
    request.setUrl("/ISteamUserAuth/AuthenticateUserTicket/v1/");
    request.addHeadValue("Host", "api.steampowered.com");

    HttpQueryParameter p;
    p.add("key", "DCD9C36F1F54A96F707DFBE833600167");
    p.add("appid", "929390");
    p.add("ticket", "140000006FC57764C95D45085373F104"
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

    for (size_t i = 0; i < 10; i++)
    {
        wrapper::HttpConnectionBuilder()
            .configureConnector(connector)
            .configureService(service)
            .configureConnectOptions({
                ConnectOption::WithAddr("23.73.140.64", 80),
                ConnectOption::WithTimeout(std::chrono::seconds(10)),
                ConnectOption::WithFailedCallback([]() {
                        std::cout << "connect failed" << std::endl;
                    }),
            })
            .configureConnectionOptions({
                AddSocketOption::WithMaxRecvBufferSize(1024),
                AddSocketOption::AddEnterCallback([](const TcpConnection::Ptr& session) {
                    // do something for session
                    (void)session;
                })
            })
            .configureEnterCallback([requestStr](const HttpSession::Ptr& session, HttpSessionHandlers& handlers) {
                (void)session;
                std::cout << "connect success" << std::endl;
                session->send(requestStr.c_str(), requestStr.size());
                handlers.setHttpCallback([requestStr](const HTTPParser& httpParser,
                                         const HttpSession::Ptr& session) {
                    (void)session;
                    std::cout << httpParser.getBody() << std::endl;
                });
            })
            .asyncConnect();
    }

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
