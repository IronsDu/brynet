#ifndef _REIDS_SSDB_COVERT_H
#define _REIDS_SSDB_COVERT_H

#include "SSDBProtocol.h"
#include "RedisRequest.h"
#include "RedisParse.h"

RedisProtocolRequest   ssdbRequestCovertToRedis(const char* buffer, int len);
SSDBProtocolResponse    redisReplyCovertToSSDB(const redisReply* redisReply);

#endif