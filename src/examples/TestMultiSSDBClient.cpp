#include <iostream>
using namespace std;

#include "SocketLibFunction.h"
#include "SSDBMultiClient.h"
#include "SSDBProtocol.h"
#include "EventLoop.h"

SSDBMultiClient::PTR ssdb;

static void async_get_callback(const std::string& value, const Status& status)
{
    ssdb->redisGet("hello", async_get_callback);

    static int total = 0;
    static int oldtime = GetTickCount();

    total++;
    int nowtime = GetTickCount();
    if ((nowtime - oldtime) >= 1000)
    {
        cout << "total ssdb get : " << total << "/s" << endl;
        oldtime = nowtime;
        total = 0;
    }
}

int main(int argc, char** argv)
{
    EventLoop mainLoop;

    ssdb = std::make_shared<SSDBMultiClient>();
    ssdb->startNetThread([&](){
        mainLoop.wakeup();
    });

    ssdb->addProxyConnection(argv[1], atoi(argv[2]));

    ssdb->redisGet("hello", [](const std::string& value, const Status& status){
        cout << value << endl;
    });

    for (size_t i = 0; i < 1000; i++)
    {
        ssdb->redisGet("hello", async_get_callback);
    }

    ssdb->qpush("mlist", "a", nullptr);

    while (true)
    {
        mainLoop.loop(1);
        ssdb->pull();
        ssdb->forceSyncRequest();
    }
}