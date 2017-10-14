#ifndef _ETCD_CLIENT_H
#define _ETCD_CLIENT_H

#include <brynet/net/http/HttpServer.h>
#include <brynet/net/http/HttpParser.h>
#include <brynet/net/http/HttpFormat.h>

using namespace dodo::net;

static HTTPParser etcdHelp(const std::string& ip, int port, HttpFormat::HTTP_TYPE_PROTOCOL protocol, const std::string& url,
    const std::map<std::string, std::string>& kv, int timeout)
{
    HTTPParser result(HTTP_BOTH);

    std::mutex mtx;
    std::condition_variable cv;

    HttpServer server;
    dodo::Timer::WeakPtr timer;
    server.startWorkThread(1);

    sock fd = ox_socket_connect(false, ip.c_str(), port);
    if (fd != SOCKET_ERROR)
    {
        server.addConnection(fd, [kv, url, &mtx, &cv, &server, &timer, timeout, protocol](HttpSession::PTR session){
            timer = server.getServer()->getService()->getRandomEventLoop()->getTimerMgr()->addTimer(timeout, [session](){
                session->postClose();
            });

            HttpFormat request;
            request.setHost("127.0.0.1");
            request.addHeadValue("Accept", "*/*");
            request.setProtocol(protocol);
            std::string keyUrl = "/v2/keys/";
            keyUrl.append(url);
            request.setRequestUrl(keyUrl.c_str());
            if (!kv.empty())
            {
                for (auto& v : kv)
                {
                    request.addParameter(v.first.c_str(), v.second.c_str());
                }
                request.setContentType("application/x-www-form-urlencoded");
            }
            std::string requestStr = request.getResult();
            session->send(requestStr.c_str(), requestStr.size());

        }, [&cv, &result, &timer](const HTTPParser& httpParser, HttpSession::PTR session){
            result = httpParser;
            session->postClose();
            if (timer.lock() != nullptr)
            {
                timer.lock()->cancel();
            }
        }, nullptr, [&cv, &timer](HttpSession::PTR session){
            if (timer.lock() != nullptr)
            {
                timer.lock()->cancel();
            }
            cv.notify_one();
        });

        std::unique_lock<std::mutex> tmp(mtx);
        cv.wait(tmp);
    }

    return result;
}

static HTTPParser etcdSet(const std::string& ip, int port, const std::string& url, const std::map<std::string, std::string>& kv, int timeout)
{
    return etcdHelp(ip, port, HttpFormat::HTP_PUT, url, kv, timeout);
}

static HTTPParser etcdGet(const std::string& ip, int port, const std::string& url, int timeout)
{
    return etcdHelp(ip, port, HttpFormat::HTP_GET, url, {}, timeout);
}

#endif