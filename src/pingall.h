#ifndef PINGALL_H
#define PINGALL_H

#define REDISMODULE_EXPERIMENTAL_API
#include "redismodule.h"

#define MSGTYPE_PING 1
#define MSGTYPE_PONG 2
#define PRALLEL_PINGS 100

int CreatePingCommands(RedisModuleCtx *ctx);

#endif /* PINGALL_H */
