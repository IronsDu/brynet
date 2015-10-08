#include <iostream>
#include <queue>
#include <assert.h>
using namespace std;

#include "SocketLibFunction.h"
#include "datasocket.h"
#include "SSDBProtocol.h"
#include "RedisRequest.h"
#include "RedisParse.h"
#include "RedisSSDBCovert.h"

#include "SSDBMultiClient.h"

SSDBMultiClient::SSDBMultiClient()
{
    mNetThread = nullptr;
    mCallbackNextID = 0;
    mRunIOLoop = false;
    mRequestProtocol = new MyRequestProcotol();
    mResponseProtocol = new SSDBProtocolResponse();
}

SSDBMultiClient::~SSDBMultiClient()
{
    if (mRequestProtocol != nullptr)
    {
        delete mRequestProtocol;
        mRequestProtocol = nullptr;
    }

    if (mResponseProtocol != nullptr)
    {
        delete mResponseProtocol;
        mResponseProtocol = nullptr;
    }
}

struct DBServerUserData
{
    queue<std::function<void(const string& response)>>* callbacklist;
    parse_tree* p;
};

void SSDBMultiClient::startNetThread(std::function<void(void)> frameCallback)
{
    if (mNetThread == nullptr)
    {
        mRunIOLoop = true;
        mNetThread = new thread([&, frameCallback](){
            while (mRunIOLoop)
            {
                mNetService.loop(1);

                mRequestList.SyncRead(0);
                RequestMsg tmp;
                while (mRequestList.PopFront(&tmp))
                {
                    if (!mProxyClients.empty())
                    {
                        /*  随机选择一个服务器   */
                        DataSocket::PTR client = mProxyClients[rand() % mProxyClients.size()];
                        DBServerUserData* dbUserData = (DBServerUserData*)client->getUserData();
                        queue<std::function<void(const string& response)>>* callbacklist = dbUserData->callbacklist;
                        callbacklist->push(tmp.callback);

                        client->send(tmp.content.c_str(), tmp.content.size());
                    }
                    else
                    {
                        /*  TODO::因为回调与请求的顺序是一一对应，所以如果遇到某个请求处理失败(包括超时，以及服务器断开连接的情况)，则需要模拟一个错误的反馈  */
                    }
                }

                mLogicFunctorMQ.ForceSyncWrite();
                /*  TODO::目前只在通知队列里有数据时，才调用帧回调--可在其中唤醒主线程--就无需把外部mainloop设置到此类   */
                if (mLogicFunctorMQ.SharedListSize() > 0 && frameCallback != nullptr)
                {
                    frameCallback();
                }
            }
        });
    }
}

void SSDBMultiClient::addProxyConnection(string ip, int port)
{
    sock fd = ox_socket_connect(ip.c_str(), port);
    ox_socket_nodelay(fd);
    SSDBMultiClient* pthis = this;

    DataSocket::PTR ds = new DataSocket(fd);

    ds->setEnterCallback([this](DataSocket::PTR ds){
        mProxyClients.push_back(ds);
        DBServerUserData* dbUserData = new DBServerUserData;
        dbUserData->callbacklist = new queue<std::function<void(const string& response)>>();
        dbUserData->p = parse_tree_new();
        ds->setUserData((int64_t)dbUserData);
    });

    ds->setDataCallback([&, pthis](DataSocket::PTR ds, const char* buffer, int len){
        const char* parse_str = buffer;
        DBServerUserData* dbUserData = (DBServerUserData*)ds->getUserData();
        while (parse_str < (buffer + len))
        {
            /*  TODO::检测是否redis协议， 并编写redis reply到ssdb reply的转换 */
            char c = parse_str[0];
            int packet_len = 0;

            if (c == '+' ||
                c == '-' ||
                c == ':' ||
                c == '$' ||
                c == '*')
            {
                if (dbUserData->p == nullptr)
                {
                    dbUserData->p = parse_tree_new();
                }

                char* parseEndPos = (char*)parse_str;
                int parseRet = parse(dbUserData->p, &parseEndPos, (char*)buffer+len);
                if (parseRet == REDIS_OK)
                {
                    packet_len = (parseEndPos - parse_str);
                }
                else if (parseRet == REDIS_RETRY)
                {

                }
                else
                {
                    assert(false);
                }

                parse_tree_del(dbUserData->p);
                dbUserData->p = nullptr;
            }
            else
            {
                packet_len = SSDBProtocolResponse::check_ssdb_packet(parse_str, (len - (parse_str - buffer)));
            }

            if (packet_len > 0)
            {
                /*  取出等待的异步回调，并将response附加给它，再投递给逻辑线程去执行    */
                queue<std::function<void(const string& response)>>* callbacklist = dbUserData->callbacklist;
                assert(!callbacklist->empty());
                if (!callbacklist->empty())
                {
                    auto& callback = callbacklist->front();
                    std::shared_ptr<string > response = std::make_shared<string>(parse_str, packet_len);
                    mLogicFunctorMQ.Push([callback, response](){
                        callback(*response);
                    });
                    callbacklist->pop();
                }

                parse_str += packet_len;
            }
            else
            {
                break;
            }
        }

        return parse_str-buffer;
    });

    ds->setDisConnectCallback([&](DataSocket::PTR arg){
        DBServerUserData* dbUserData = (DBServerUserData*)arg->getUserData();
        delete dbUserData->callbacklist;
        if (dbUserData->p != nullptr)
        {
            parse_tree_del(dbUserData->p);
        }
        delete dbUserData;
        /*  TODO::当服务器断开连接后，要处理pending在它上面的请求，要处理异步回调的失败情况   */
        for (size_t i = 0; i < mProxyClients.size(); ++i)
        {
            if (mProxyClients[i] == arg)
            {
                mProxyClients.erase(mProxyClients.begin() + i);
            }
        }
        delete arg;
    });


    mNetService.pushAsyncProc([&, ds](){
        if (!ds->onEnterEventLoop(&mNetService))
        {
            delete ds;
        }
    });
}

/*  逻辑线程里处理response，无论什么协议，都将数据转换到ssdb格式；如果是redis协议，则返回非空的parse_tree*,否则返回nullptr    */
parse_tree* SSDBMultiClient::processResponse(const string& response)
{
    mResponseProtocol->init();
    char c = response.front();

    if (c == '+' ||
        c == '-' ||
        c == ':' ||
        c == '$' ||
        c == '*')
    {
        parse_tree* p = parse_tree_new();
        char* parsePos = (char*)response.c_str();
        parse(p, &parsePos, (char*)response.c_str()+response.size());
        *mResponseProtocol = redisReplyCovertToSSDB(p->reply);
        return p;
    }
    else
    {
        mResponseProtocol->parse(response.c_str());
    }

    return nullptr;
}

void SSDBMultiClient::pushNoneValueRequest(const char* request, int len, const NONE_VALUE_CALLBACK& callback)
{
    RequestMsg msg([this, callback](const std::string& response){
        if (callback != nullptr)
        {
            parse_tree* p = processResponse(response);
            callback(mResponseProtocol->getStatus());
            if (p != nullptr)
            {
                parse_tree_del(p);
            }
        }
        
    }, std::string(request, len) );

    mRequestList.Push(std::move(msg));
}

void SSDBMultiClient::pushStringValueRequest(const char* request, int len, const ONE_STRING_CALLBACK& callback)
{
    RequestMsg msg([this, callback](const std::string& response){
        if (callback != nullptr)
        {
            parse_tree* p = processResponse(response);
            string ret;
            Status s = read_str(mResponseProtocol, &ret);
            callback(ret, s);
            if (p != nullptr)
            {
                parse_tree_del(p);
            }
        }
    }, std::string(request, len));

    mRequestList.Push(std::move(msg));
}

void SSDBMultiClient::pushStringListRequest(const char* request, int len, const STRING_LIST_CALLBACK& callback)
{
    RequestMsg msg([this, callback](const std::string& response){
        if (callback != nullptr)
        {
            parse_tree* p = processResponse(response);
            std::vector<string> ret;
            Status s = read_list(mResponseProtocol, &ret);
            callback(ret, s);
            if (p != nullptr)
            {
                parse_tree_del(p);
            }
        }
    }, std::string(request, len));

    mRequestList.Push(std::move(msg));
}

void SSDBMultiClient::pushIntValueRequest(const char* request, int len, const ONE_INT64_CALLBACK& callback)
{
    RequestMsg msg([this, callback](const std::string& response){
        if (callback != nullptr)
        {
            parse_tree* p = processResponse(response);
            int64_t ret;
            Status s = read_int64(mResponseProtocol, &ret);
            callback(ret, s);
            if (p != nullptr)
            {
                parse_tree_del(p);
            }
        }
    }, std::string(request, len));

    mRequestList.Push(std::move(msg));
}

void SSDBMultiClient::redisSet(const std::string& key, const std::string& value, const NONE_VALUE_CALLBACK& callback)
{
    RedisProtocolRequest req;
    req.writev("SET", key, value);
    req.endl();

    pushNoneValueRequest(req.getResult(), req.getResultLen(), callback);
}

void SSDBMultiClient::redisGet(const std::string& key, const ONE_STRING_CALLBACK& callback)
{
#if 1
    char tmp[1024];
    int len = sprintf(tmp, "*%d\r\n$%d\r\n%s\r\n$%d\r\n%s\r\n", 2, 3, "GET", key.size(), key.c_str());

    pushStringValueRequest(tmp, len, callback);
#else
    RedisProtocolRequest req;
    req.writev("GET", key);
    req.endl();

    pushStringValueRequest(req.getResult(), req.getResultLen(), callback);
#endif
}

void SSDBMultiClient::multiSet(const std::unordered_map<std::string, std::string> &kvs, const NONE_VALUE_CALLBACK& callback)
{
    mRequestProtocol->init();
    mRequestProtocol->writev("multi_set", kvs);
    mRequestProtocol->endl();

    pushNoneValueRequest(mRequestProtocol->getResult(), mRequestProtocol->getResultLen(), callback);
}

void SSDBMultiClient::multiGet(const std::vector<std::string> &keys, const STRING_LIST_CALLBACK& callback)
{
    mRequestProtocol->init();
    mRequestProtocol->writev("multi_get", keys);
    mRequestProtocol->endl();

    pushStringListRequest(mRequestProtocol->getResult(), mRequestProtocol->getResultLen(), callback);
}

void SSDBMultiClient::multiDel(const std::vector<std::string> &keys, const NONE_VALUE_CALLBACK& callback)
{
    mRequestProtocol->init();
    mRequestProtocol->writev("multi_del", keys);
    mRequestProtocol->endl();

    pushNoneValueRequest(mRequestProtocol->getResult(), mRequestProtocol->getResultLen(), callback);
}

void SSDBMultiClient::set(const std::string& key, const std::string& value, const NONE_VALUE_CALLBACK& callback)
{
    mRequestProtocol->init();
    mRequestProtocol->writev("set", key, value);
    mRequestProtocol->endl();

    pushNoneValueRequest(mRequestProtocol->getResult(), mRequestProtocol->getResultLen(), callback);
}

void SSDBMultiClient::get(const string& k, const ONE_STRING_CALLBACK& callback)
{
    mRequestProtocol->init();
    mRequestProtocol->writev("get", k);
    mRequestProtocol->endl();

    pushStringValueRequest(mRequestProtocol->getResult(), mRequestProtocol->getResultLen(), callback);
}

void SSDBMultiClient::hget(const string& hname, const string& k, const ONE_STRING_CALLBACK& callback)
{
    mRequestProtocol->init();
    mRequestProtocol->writev("hget", hname, k);
    mRequestProtocol->endl();

    pushStringValueRequest(mRequestProtocol->getResult(), mRequestProtocol->getResultLen(), callback);
}

void SSDBMultiClient::hset(const string& hname, const string& k, const string& v, const NONE_VALUE_CALLBACK& callback)
{
    mRequestProtocol->init();
    mRequestProtocol->writev("hset", hname, k, v);
    mRequestProtocol->endl();

    pushNoneValueRequest(mRequestProtocol->getResult(), mRequestProtocol->getResultLen(), callback);
}

void SSDBMultiClient::multiHget(const string& hname, const std::vector<std::string> &keys, const STRING_LIST_CALLBACK& callback)
{
    mRequestProtocol->init();
    mRequestProtocol->writev("multi_hget", hname, keys);
    mRequestProtocol->endl();

    pushStringListRequest(mRequestProtocol->getResult(), mRequestProtocol->getResultLen(), callback);
}

void SSDBMultiClient::multiHset(const string& hname, const std::unordered_map<std::string, std::string> &kvs, const NONE_VALUE_CALLBACK& callback)
{
    mRequestProtocol->init();
    mRequestProtocol->writev("multi_hset", hname, kvs);
    mRequestProtocol->endl();

    pushNoneValueRequest(mRequestProtocol->getResult(), mRequestProtocol->getResultLen(), callback);
}

void SSDBMultiClient::zset(const std::string& name, const std::string& key, int64_t score,
                           const NONE_VALUE_CALLBACK& callback)
{
    mRequestProtocol->init();
    mRequestProtocol->writev("zset", name, key, score);
    mRequestProtocol->endl();

    pushNoneValueRequest(mRequestProtocol->getResult(), mRequestProtocol->getResultLen(), callback);
}

void SSDBMultiClient::zget(const std::string& name, const std::string& key, const ONE_INT64_CALLBACK& callback)
{
    mRequestProtocol->init();
    mRequestProtocol->writev("zget", name, key);
    mRequestProtocol->endl();

    pushIntValueRequest(mRequestProtocol->getResult(), mRequestProtocol->getResultLen(), callback);
}

void SSDBMultiClient::zsize(const std::string& name, const ONE_INT64_CALLBACK& callback)
{
    mRequestProtocol->init();
    mRequestProtocol->writev("zsize", name);
    mRequestProtocol->endl();

    pushIntValueRequest(mRequestProtocol->getResult(), mRequestProtocol->getResultLen(), callback);
}

void SSDBMultiClient::zkeys(const std::string& name, const std::string& key_start, int64_t score_start, int64_t score_end,
                            uint64_t limit, const STRING_LIST_CALLBACK& callback)
{
    mRequestProtocol->init();
    mRequestProtocol->writev("zkeys", name, key_start, score_start, score_end);
    mRequestProtocol->endl();

    pushStringListRequest(mRequestProtocol->getResult(), mRequestProtocol->getResultLen(), callback);
}

void SSDBMultiClient::zscan(const std::string& name, const std::string& key_start, int64_t score_start, int64_t score_end,
                            uint64_t limit, const STRING_LIST_CALLBACK& callback)
{
    mRequestProtocol->init();
    mRequestProtocol->writev("zscan", name, key_start, score_start, score_end, limit);

    pushStringListRequest(mRequestProtocol->getResult(), mRequestProtocol->getResultLen(), callback);
}

void SSDBMultiClient::zclear(const std::string& name, const NONE_VALUE_CALLBACK& callback)
{
    mRequestProtocol->init();
    mRequestProtocol->writev("zclear", name);
    mRequestProtocol->endl();

    pushNoneValueRequest(mRequestProtocol->getResult(), mRequestProtocol->getResultLen(), callback);
}

void SSDBMultiClient::qpush(const std::string& name, const std::string& item, const NONE_VALUE_CALLBACK& callback)
{
    mRequestProtocol->init();
    mRequestProtocol->writev("qpush", name, item);
    mRequestProtocol->endl();

    pushNoneValueRequest(mRequestProtocol->getResult(), mRequestProtocol->getResultLen(), callback);
}

void SSDBMultiClient::qpop(const std::string& name, const ONE_STRING_CALLBACK& callback)
{
    mRequestProtocol->init();
    mRequestProtocol->writev("qpop", name);
    mRequestProtocol->endl();

    pushStringValueRequest(mRequestProtocol->getResult(), mRequestProtocol->getResultLen(), callback);
}

void SSDBMultiClient::qslice(const std::string& name, int64_t begin, int64_t end, const STRING_LIST_CALLBACK& callback)
{
    mRequestProtocol->init();
    mRequestProtocol->writev("qslice", name, begin, end);
    mRequestProtocol->endl();

    pushStringListRequest(mRequestProtocol->getResult(), mRequestProtocol->getResultLen(), callback);
}

void SSDBMultiClient::qclear(const std::string& name, const NONE_VALUE_CALLBACK& callback)
{
    mRequestProtocol->init();
    mRequestProtocol->writev("qclear", name);
    mRequestProtocol->endl();

    pushNoneValueRequest(mRequestProtocol->getResult(), mRequestProtocol->getResultLen(), callback);
}

void SSDBMultiClient::forceSyncRequest()
{
    mRequestList.ForceSyncWrite();
    if (mRequestList.SharedListSize() > 0)
    {
        mNetService.wakeup();
    }
}

void SSDBMultiClient::pull()
{
    mLogicFunctorMQ.SyncRead(0);
    std::function<void(void)> tmp;
    while (mLogicFunctorMQ.PopFront(&tmp))
    {
        tmp();
    }
}

void SSDBMultiClient::stopService()
{
    if (mNetThread != nullptr)
    {
        mRunIOLoop = false;
        mNetService.wakeup();
        mNetThread->join();
        delete mNetThread;
        mNetThread = nullptr;
    }

    mProxyClients.clear();

    mOneStringValueCallback.clear();
    mNoValueCallback.clear();
    mOntInt64Callback.clear();
    mStringListCallback.clear();
}