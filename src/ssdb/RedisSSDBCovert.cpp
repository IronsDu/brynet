#include <vector>
#include <string>
using namespace std;

#include "RedisParse.h"
#include "RedisRequest.h"
#include "SSDBProtocol.h"

RedisProtocolRequest   ssdbRequestCovertToRedis(const char* buffer, int len)
{
    SSDBProtocolResponse ssdbRequest;
    ssdbRequest.parse(buffer);

    RedisProtocolRequest ret;

    vector<string> tmp;
    for (size_t i = 0; i < ssdbRequest.getBuffersLen(); ++i)
    {
        Bytes* b = ssdbRequest.getByIndex(i);
        tmp.push_back(string(b->buffer, b->len));
    }

    ret.writev(tmp);

    return ret;
}

static void  covertRedisReplyToSSDB(SSDBProtocolResponse& ret, const redisReply* redisReply)
{
    switch (redisReply->type)
    {
        case REDIS_REPLY_STRING:
            ret.pushByte(redisReply->str, redisReply->len);
            break;
        case REDIS_REPLY_ARRAY:
            for (size_t i = 0; i < redisReply->elements; ++i)
            {
                covertRedisReplyToSSDB(ret, redisReply->element[i]);
            }
            break;
        case REDIS_REPLY_INTEGER:
            ret.pushByte(redisReply->str, redisReply->len);
            break;
        case REDIS_REPLY_NIL:
            ret.pushByte("", 0);
            break;
        case REDIS_REPLY_STATUS:
            ret.pushByte(redisReply->str, strlen(redisReply->str));
            break;
        case REDIS_REPLY_ERROR:
            ret.pushByte(redisReply->str, redisReply->len);
            break;
        default:
            break;
    }
}

SSDBProtocolResponse redisReplyCovertToSSDB(const redisReply* redisReply)
{
    SSDBProtocolResponse ret;
    if (redisReply->type == REDIS_REPLY_STRING ||
        redisReply->type == REDIS_REPLY_ARRAY ||
        redisReply->type == REDIS_REPLY_INTEGER)
    {
        ret.pushByte("ok", 2);
    }
    else if (redisReply->type == REDIS_REPLY_NIL)
    {
        ret.pushByte("not_found", 9); 
    }

    covertRedisReplyToSSDB(ret, redisReply);
    return ret;
}