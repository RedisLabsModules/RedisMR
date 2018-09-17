#define REDISMODULE_EXPERIMENTAL_API

#include "redismodule.h"
#include "aval.h"
#include "list.h"
#include "pingall.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

/* Register the commands to Redis server. */
int RedisModule_OnLoad(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    REDISMODULE_NOT_USED(argv);
    REDISMODULE_NOT_USED(argc);

    if (RedisModule_Init(ctx,"rmr",1,REDISMODULE_APIVER_1) == REDISMODULE_ERR)
    	return REDISMODULE_ERR;

    if (CreatePingCommands(ctx) == REDISMODULE_ERR)
        return REDISMODULE_ERR;

    if (CreateAvalCommands(ctx) == REDISMODULE_ERR)
        return REDISMODULE_ERR;

    if (CreateListCommands(ctx) == REDISMODULE_ERR)
        return REDISMODULE_ERR;

    return REDISMODULE_OK;
}
