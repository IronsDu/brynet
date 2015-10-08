#ifndef _SSDB_PROXY_CLIENT_H
#define _SSDB_PROXY_CLIENT_H

#include <string>
#include <functional>
#include <unordered_map>
#include <stdint.h>
#include <vector>
#include <thread>
#include <memory>

#include "eventloop.h"
#include "datasocket.h"
#include "msgqueue.h"
#include "SSDBProtocol.h"

using namespace std;

class DataSocket;
class SSDBProtocolRequest;
class RedisProtocolRequest;
struct parse_tree;

/*  可链接多个ssdb 服务器的客户端   */

/*  TODO::目前对于ssdb请求是随机选择服务器，后续或许也需要提供重载让使用者去管理链接，以及客户端自定义sharding    */

/*  TODO::目前主要支持ssdb；而redis只支持get/set。且redis和ssdb的转换也没有测试   */

class SSDBMultiClient
{
public:
    typedef SSDBProtocolRequest MyRequestProcotol;

    typedef std::shared_ptr<SSDBMultiClient> PTR;

    typedef std::function<void(const std::string&, const Status&)>  ONE_STRING_CALLBACK;
    typedef std::function<void(const Status&)>                      NONE_VALUE_CALLBACK;
    typedef std::function<void(int64_t, const Status&)>             ONE_INT64_CALLBACK;
    typedef std::function<void(const std::vector<std::string>&, const Status&)> STRING_LIST_CALLBACK;

public:
    SSDBMultiClient();
    ~SSDBMultiClient();

    void                                                            startNetThread(std::function<void(void)> frameCallback = nullptr);
    void                                                            addProxyConnection(string ip, int port);

    void                                                            redisSet(const std::string& key, const std::string& value, const NONE_VALUE_CALLBACK& callback);
    void                                                            redisGet(const std::string& key, const ONE_STRING_CALLBACK& callback);

    void                                                            multiSet(const std::unordered_map<std::string, std::string> &kvs, const NONE_VALUE_CALLBACK& callback);
    void                                                            multiGet(const std::vector<std::string> &keys, const STRING_LIST_CALLBACK& callback);
    void                                                            multiDel(const std::vector<std::string> &keys, const NONE_VALUE_CALLBACK& callback);

    void                                                            set(const std::string& key, const std::string& value, const NONE_VALUE_CALLBACK& callback);
    void                                                            get(const string& k, const ONE_STRING_CALLBACK& callback);

    void                                                            hget(const string& hname, const string& k, const ONE_STRING_CALLBACK& callback);
    void                                                            hset(const string& hname, const string& k, const string& v, const NONE_VALUE_CALLBACK& callback);

    void                                                            multiHget(const string& hname, const std::vector<std::string> &keys, const STRING_LIST_CALLBACK& callback);
    void                                                            multiHset(const string& hname, const std::unordered_map<std::string, std::string> &kvs, const NONE_VALUE_CALLBACK& callback);

    void                                                            zset(const std::string& name, const std::string& key, int64_t score,
                                                                        const NONE_VALUE_CALLBACK& callback);

    void                                                            zget(const std::string& name, const std::string& key, const ONE_INT64_CALLBACK& callback);

    void                                                            zsize(const std::string& name, const ONE_INT64_CALLBACK& callback);

    void                                                            zkeys(const std::string& name, const std::string& key_start, int64_t score_start, int64_t score_end,
                                                                        uint64_t limit, const STRING_LIST_CALLBACK& callback);

    void                                                            zscan(const std::string& name, const std::string& key_start, int64_t score_start, int64_t score_end,
                                                                        uint64_t limit, const STRING_LIST_CALLBACK& callback);

    void                                                            zclear(const std::string& name, const NONE_VALUE_CALLBACK& callback);

    void                                                            qpush(const std::string& name, const std::string& item, const NONE_VALUE_CALLBACK&);
    void                                                            qpop(const std::string& name, const ONE_STRING_CALLBACK&);
    void                                                            qslice(const std::string& name, int64_t begin, int64_t end, const STRING_LIST_CALLBACK& callback);
    void                                                            qclear(const std::string& name, const NONE_VALUE_CALLBACK& callback);

    void                                                            forceSyncRequest();
    void                                                            pull();
    void                                                            stopService();

private:
    /*投递没有返回值的db请求*/
    void                                                            pushNoneValueRequest(const char* request, int len, const NONE_VALUE_CALLBACK& callback);
    /*投递返回值为string的db请求*/
    void                                                            pushStringValueRequest(const char* request, int len, const ONE_STRING_CALLBACK& callback);
    /*投递返回值为string list的db请求*/
    void                                                            pushStringListRequest(const char* request, int len, const STRING_LIST_CALLBACK& callback);
    /*投递返回值为int64_t的db请求*/
    void                                                            pushIntValueRequest(const char* request, int len, const ONE_INT64_CALLBACK& callback);

    parse_tree*                                                     processResponse(const string& response);
private:
    std::thread*                                                    mNetThread;
    int64_t                                                         mCallbackNextID;

    unordered_map<int64_t, ONE_STRING_CALLBACK>                     mOneStringValueCallback;
    unordered_map<int64_t, NONE_VALUE_CALLBACK>                     mNoValueCallback;
    unordered_map<int64_t, ONE_INT64_CALLBACK>                      mOntInt64Callback;
    unordered_map<int64_t, STRING_LIST_CALLBACK>                    mStringListCallback;

    bool                                                            mRunIOLoop;
    EventLoop                                                       mNetService;
    vector<DataSocket::PTR>                                         mProxyClients;

    /*投递到网络线程的db请求*/
    struct RequestMsg
    {
        RequestMsg()
        {}

        RequestMsg(const std::function<void(const string&)>& acallback, const string& acontent) : callback(acallback), content(acontent)
        {
        }

        RequestMsg(std::function<void(const string&)>&& acallback, string&& acontent) : callback(std::move(acallback)), content(std::move(acontent))
        {
        }

        RequestMsg(RequestMsg &&a) : callback(std::move(a.callback)), content(std::move(a.content))
        {
        }

        RequestMsg & operator = (RequestMsg &&a)
        {
            if (this != &a)
            {
                callback = std::move(a.callback);
                content = std::move(a.content);
            }

            return *this;
        }
        std::function<void(const string&)> callback;    /*用户线程的异步回调*/
        string  content;                                /*请求的协议内容*/
    };

    MsgQueue<RequestMsg>                                            mRequestList;
    MsgQueue<std::function<void(void)>>                             mLogicFunctorMQ;

    MyRequestProcotol*                                              mRequestProtocol;
    SSDBProtocolResponse*                                           mResponseProtocol;
};

#endif