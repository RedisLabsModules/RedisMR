#include "pingall.h"

typedef struct Result{
	int numnodes;
	int count;
	RedisModuleBlockedClient* client;
} PingResult;

static int pingId = 0;
static PingResult clients[PRALLEL_PINGS];

/* Reply callback for blocking command rmr.pingall */
int PingBlock_Reply(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    REDISMODULE_NOT_USED(argv);
    REDISMODULE_NOT_USED(argc);

    printf("HelloBlock_Reply");

    int *myint = RedisModule_GetBlockedClientPrivateData(ctx);
//    return 0;
    return RedisModule_ReplyWithLongLong(ctx,*myint);

//    return RedisModule_ReplyWithLongLong(ctx,1);

}

/* Timeout callback for blocking command rmr.pingall*/
int PingBlock_Timeout(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    REDISMODULE_NOT_USED(argv);
    REDISMODULE_NOT_USED(argc);
    return RedisModule_ReplyWithSimpleString(ctx,"Request timedout");
}

/* Private data freeing callback for rmr.ping command. */
void PingBlock_FreeData(RedisModuleCtx *ctx, void *privdata) {
    REDISMODULE_NOT_USED(ctx);
    RedisModule_Free(privdata);
}

/* An example blocked client disconnection callback.
 *
 * Note that in the case of the rmr.ping command, the blocked client is now
 * owned by the thread calling sleep(). In this specific case, there is not
 * much we can do, however normally we could instead implement a way to
 * signal the thread that the client disconnected, and sleep the specified
 * amount of seconds with a while loop calling sleep(1), so that once we
 * detect the client disconnection, we can terminate the thread ASAP. */
void PingBlock_Disconnected(RedisModuleCtx *ctx, RedisModuleBlockedClient *bc) {
    RedisModule_Log(ctx,"warning","Blocked client %p disconnected!",
        (void*)bc);

    /* Here you should cleanup your state / threads, and if possible
     * call RedisModule_UnblockClient(), or notify the thread that will
     * call the function ASAP. */
}

/* rmr.PINGALL */
int PingallCommand_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
//    REDISMODULE_NOT_USED(argv);
//    REDISMODULE_NOT_USED(argc);

    size_t numnodes = RedisModule_GetClusterSize();
    if (numnodes < 2) {
        return RedisModule_ReplyWithError(ctx,"Cluster not enabled");
    }

    long long timeout = 1000;
    if (argc > 1 && RedisModule_StringToLongLong(argv[1],&timeout) != REDISMODULE_OK) {
        return RedisModule_ReplyWithError(ctx,"ERR invalid count");
    }

    RedisModuleBlockedClient *bc = RedisModule_BlockClient(ctx,PingBlock_Reply,PingBlock_Timeout,PingBlock_FreeData,timeout);

    /* Here we set a disconnection handler */
    RedisModule_SetDisconnectCallback(bc,PingBlock_Disconnected);

    clients[pingId].client = bc;
    clients[pingId].numnodes = numnodes;
    clients[pingId].count = 1; // starts from 1 to include SELF

    char snum[3];
    sprintf(snum, "%02d", pingId);

	pingId = (pingId + 1) % PRALLEL_PINGS;

//    RedisModule_SendClusterMessage(ctx,NULL,MSGTYPE_PING,(unsigned char*)"Hey",3);

    RedisModule_SendClusterMessage(ctx,NULL,MSGTYPE_PING,(unsigned char*)snum, strlen(snum));

//    return RedisModule_ReplyWithSimpleString(ctx, "OK");
    return REDISMODULE_OK;
}

/* Callback for message MSGTYPE_PING */
void PingReceiver(RedisModuleCtx *ctx, const char *sender_id, uint8_t type, const unsigned char *payload, uint32_t len) {
    RedisModule_Log(ctx,"notice","PING (type %d) RECEIVED from %.*s: '%.*s'",
        type,REDISMODULE_NODE_ID_LEN,sender_id,(int)len, payload);
    RedisModule_SendClusterMessage(ctx,sender_id,MSGTYPE_PONG, payload,len);
}

/* Callback for message MSGTYPE_PONG. */
void PongReceiver(RedisModuleCtx *ctx, const char *sender_id, uint8_t type, const unsigned char *payload, uint32_t len) {

    RedisModule_Log(ctx,"notice","PONG (type %d) RECEIVED from %.*s: '%.*s'",
        type,REDISMODULE_NODE_ID_LEN,sender_id,(int)len, payload);

	int pingId = atoi ((char*)payload);
	++clients[pingId].count;

	if( clients[pingId].count >= clients[pingId].numnodes) {
		RedisModuleBlockedClient *bc = clients[pingId].client;

		int *count = RedisModule_Alloc(sizeof(int));
		*count = clients[pingId].count;
		RedisModule_UnblockClient(bc, count);
		clients[pingId].client = NULL;
	}
}

int CreatePingCommands(RedisModuleCtx *ctx){
    if (RedisModule_CreateCommand(ctx,"rmr.pingall", PingallCommand_RedisCommand,"readonly",0,0,0) == REDISMODULE_ERR)
        return REDISMODULE_ERR;

    RedisModule_RegisterClusterMessageReceiver(ctx,MSGTYPE_PING,PingReceiver);
    RedisModule_RegisterClusterMessageReceiver(ctx,MSGTYPE_PONG,PongReceiver);

    return REDISMODULE_OK;
}
