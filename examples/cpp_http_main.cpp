#include "webclient.h"
#include "httprequest.h"
#include "cpp_server.h"

int main()
{
    CppServer cserver;

    cserver.create(1, 100024, [](CppServer&, void* ud, const char* buffer, int len)
    {
        return HttpHelp::check_packet(buffer, len);
    });

    cserver.setMsgHandle([](CppServer& cserver, struct nrmgr_net_msg* msg){
        if (msg->type == nrmgr_net_msg_connect)
        {
            /*  发出http请求    */
            HttpRequest request;
            request.setHost("www.baidu.com");
            request.setProtocol(HRP_GET);

            string request_str = request.getResult();

            struct nrmgr_send_msg_data* sdmsg = cserver.makeSendMsg(request_str.c_str(), request_str.length());
            cserver.sendMsg(msg->session, sdmsg);
        }
        else if (msg->type == nrmgr_net_msg_data)
        {
            /*  接收到数据   */

        }
    });

    HttpConnector   hc(&cserver);
    string baidu_ip = HttpHelp::getipofhost("www.baidu.com");
    hc.connect(baidu_ip.c_str(), 80, 10000, NULL);

    while (true)
    {
        hc.poll();
        cserver.logicPoll(1);
    }
    /*
    int a = 100*(1/4);
    WebClientMgr<MyHttpClient> clientMgr;

    clientMgr.init(500, 1, 10024, 10024, WebClientMgr<MyHttpClient>::s_check);
    
    connector.setSocketMgr(clientMgr.getMgr());

    string baidu_ip = HttpHelp::getipofhost("www.baidu.com");

    connector.connect(baidu_ip.c_str(), 80, 10000, NULL);

    while(true)
    {
        clientMgr.poll(1);
        connector.poll();
    }
    */

    return 0;
}