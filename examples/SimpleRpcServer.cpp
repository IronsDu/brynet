#include <iostream>
#include <string>

#include "SocketLibFunction.h"
#include "HttpServer.h"
#include "HttpFormat.h"
#include "WebSocketFormat.h"
#include "google/protobuf/service.h"
#include "google/protobuf/message.h"
#include "google/protobuf/util/json_util.h"
#include "echo_service.pb.h"
#include "WrapTCPService.h"

using namespace dodo;
using namespace dodo::net;

class TypeService
{
public:
    typedef std::shared_ptr<TypeService>    PTR;

    TypeService(std::shared_ptr<google::protobuf::Service> service) : mService(service)
    {}
    virtual ~TypeService()
    {}

    std::shared_ptr<google::protobuf::Service>  getService()
    {
        return  mService;
    }

    void    registerMethod(const ::google::protobuf::MethodDescriptor* desc)
    {
        mMethods[desc->name()] = desc;
    }

    const ::google::protobuf::MethodDescriptor* findMethod(std::string name)
    {
        auto it = mMethods.find(name);
        if (it != mMethods.end())
        {
            return (*it).second;
        }

        return nullptr;
    }

private:
    std::shared_ptr<google::protobuf::Service>  mService;
    std::unordered_map<std::string, const ::google::protobuf::MethodDescriptor*>   mMethods;
};

class RPCServiceMgr
{
public:
    typedef std::shared_ptr<RPCServiceMgr> PTR;
    
    virtual ~RPCServiceMgr()
    {}

    bool    registerService(std::shared_ptr<google::protobuf::Service> service)
    {
        auto typeService = std::make_shared<TypeService>(service);

        auto serviceDesc = service->GetDescriptor();
        for (auto i = 0; i < serviceDesc->method_count(); i++)
        {
            auto methodDesc = serviceDesc->method(i);
            typeService->registerMethod(methodDesc);
        }

        mServices[service->GetDescriptor()->full_name()] = typeService;

        return true;
    }

    TypeService::PTR    findService(const std::string& name)
    {
        auto it = mServices.find(name);
        if (it != mServices.end())
        {
            return (*it).second;
        }

        return nullptr;
    }

private:
    std::unordered_map<std::string, TypeService::PTR>   mServices;
};

class MyClosure : public ::google::protobuf::Closure
{
public:
    MyClosure(std::function<void(void)> callback) : mCallback(callback), mValid(true)
    {}

private:
    void    Run() override final
    {
        assert(mValid);
        assert(mCallback);

        //避免多次调用Run函数
        if (mValid && mCallback)
        {
            mCallback();
            mValid = false;
        }
    }

private:
    std::function<void(void)>   mCallback;
    bool                        mValid;
};

static void processHTTPRPCRequest(RPCServiceMgr::PTR rpcServiceMgr, HttpSession::PTR session, const std::string & serviceName, const std::string& methodName, const std::string& body)
{
    auto typeService = rpcServiceMgr->findService(serviceName);
    if (typeService != nullptr)
    {
        auto method = typeService->findMethod(methodName);
        if (method != nullptr)
        {
            auto requestMsg = typeService->getService()->GetRequestPrototype(method).New();
            requestMsg->ParseFromArray(body.c_str(), body.size());

            auto responseMsg = typeService->getService()->GetResponsePrototype(method).New();

            auto clouse = new MyClosure([=](){
                //将结果通过HTTP协议返回给客户端
                //TODO::delete clouse

                std::string jsonMsg;
                google::protobuf::util::MessageToJsonString(*responseMsg, &jsonMsg);

                HttpResponse httpResponse;
                httpResponse.setStatus(HttpResponse::HTTP_RESPONSE_STATUS::OK);
                httpResponse.setContentType("application/json");
                httpResponse.setBody(jsonMsg.c_str());

                auto result = httpResponse.getResult();
                session->send(result.c_str(), result.size(), nullptr);

                /*  TODO::不断开连接,由可信客户端主动断开,或者底层开始心跳检测(断开恶意连接) */
                session->postShutdown();

                delete requestMsg;
                delete responseMsg;
            });

            //TODO::support controller(超时以及错误处理)
            typeService->getService()->CallMethod(method, nullptr, requestMsg, responseMsg, clouse);
        }
    }
}

//  实现Echo服务
class MyEchoService : public sofa::pbrpc::test::EchoServer
{
public:
    MyEchoService() : mIncID(0)
    {
    }

private:
    void Echo(::google::protobuf::RpcController* controller,
        const ::sofa::pbrpc::test::EchoRequest* request,
        ::sofa::pbrpc::test::EchoResponse* response,
        ::google::protobuf::Closure* done) override
    {
        mIncID++;
        response->set_message(std::to_string(mIncID) + " is the current id");
        done->Run();
    }

private:
    int     mIncID;
};

int main(int argc, char **argv)
{
    RPCServiceMgr::PTR rpcServiceMgr = std::make_shared<RPCServiceMgr>();
    rpcServiceMgr->registerService(std::make_shared<MyEchoService>());

    HttpServer server;

    server.startListen(false, "0.0.0.0", 8080);
    server.startWorkThread(ox_getcpunum());

    server.setEnterCallback([=](HttpSession::PTR& session){
        session->setHttpCallback([=](const HTTPParser& httpParser, HttpSession::PTR session){
            auto queryPath = httpParser.getPath();
            std::string::size_type pos = queryPath.rfind('.');
            if (pos != std::string::npos)
            {
                auto serviceName = queryPath.substr(1, pos - 1);
                auto methodName = queryPath.substr(pos + 1);
                processHTTPRPCRequest(rpcServiceMgr, session, serviceName, methodName, httpParser.getBody());
            }
        });

        session->setWSCallback([=](HttpSession::PTR session, WebSocketFormat::WebSocketFrameType opcode, const std::string& payload){
            //TODO::support websocket协议,payload和二进制协议一样
        });
    });

    std::cin.get();
    return 0;
}
