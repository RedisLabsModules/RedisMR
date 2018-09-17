#ifndef AVAL_H
#define AVAL_H

#define REDISMODULE_EXPERIMENTAL_API
#include "redismodule.h"

#define PRALLEL_PINGS 100
#define KEY_SIZE 128
#define MSGTYPE_EVAL 3
#define MSGTYPE_EVAL_RES 4

//#define AVAL_KEY_PREFIX "aval:"
//#define AVAL_KEY_FMT AVAL_KEY_PREFIX "%s"

int CreateAvalCommands(RedisModuleCtx *ctx);

#endif /* AVAL_H */
