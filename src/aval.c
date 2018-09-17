#include "aval.h"

typedef struct Result{
	int numnodes;
	int count;
	char* key;
	RedisModuleBlockedClient* client;
} AvalResult;

static int pingId = 0;
static AvalResult clients[PRALLEL_PINGS];



/* Reply callback for blocking command rmr.pingall */
int EvalBlock_Reply(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    REDISMODULE_NOT_USED(argv);
    REDISMODULE_NOT_USED(argc);

    printf("HelloBlock_Reply");

    int *myint = RedisModule_GetBlockedClientPrivateData(ctx);
//    return 0;
    return RedisModule_ReplyWithLongLong(ctx,*myint);

//    return RedisModule_ReplyWithLongLong(ctx,1);

}

/* Timeout callback for blocking command rmr.pingall*/
int EvalBlock_Timeout(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    REDISMODULE_NOT_USED(argv);
    REDISMODULE_NOT_USED(argc);
    return RedisModule_ReplyWithSimpleString(ctx,"Request timedout");
}

/* Private data freeing callback for rmr.ping command. */
void EvalBlock_FreeData(RedisModuleCtx *ctx, void *privdata) {
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
void EvalBlock_Disconnected(RedisModuleCtx *ctx, RedisModuleBlockedClient *bc) {
    RedisModule_Log(ctx,"warning","Blocked client %p disconnected!",
        (void*)bc);

    /* Here you should cleanup your state / threads, and if possible
     * call RedisModule_UnblockClient(), or notify the thread that will
     * call the function ASAP. */
}

/*
 * rmr.Eval
 * EVAL script numkeys key [key ...] arg [arg ...]
 */
int EvalCommand_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
//    REDISMODULE_NOT_USED(argv);
//    REDISMODULE_NOT_USED(argc);

	RedisModule_AutoMemory(ctx); /* Use automatic memory management. */


    size_t numnodes = RedisModule_GetClusterSize();
    if (numnodes < 2) {
        return RedisModule_WrongArity(ctx);
    }

    long long timeout = 1000;
    if (argc < 3) {
        return RedisModule_ReplyWithError(ctx,"ERR invalid args");
    }

    RedisModuleBlockedClient *bc = RedisModule_BlockClient(ctx,EvalBlock_Reply,EvalBlock_Timeout,EvalBlock_FreeData,timeout);

    /* TODO Here we set a disconnection handler */
    RedisModule_SetDisconnectCallback(bc,EvalBlock_Disconnected);


//	RedisModule_HashSet(dst,REDISMODULE_HASH_NX|REDISMODULE_HASH_CFIELDS,"");


    clients[pingId].client = bc;
    clients[pingId].numnodes = numnodes;
    clients[pingId].count = 1; // starts from 1 to include SELF

	char *key = RedisModule_Alloc(sizeof(char)*(KEY_SIZE + 1));
	RedisModule_GetRandomHexChars(key, KEY_SIZE);
    clients[pingId].key = key;

    size_t scriptLength;
    const char* script = RedisModule_StringPtrLen(argv[1], &scriptLength);

    char snum[scriptLength + 3];
    sprintf(snum, "%s%02d", script, pingId);

	pingId = (pingId + 1) % PRALLEL_PINGS;




//	RedisModuleString *keyName = RedisModule_CreateStringPrintf(ctx, AVAL_KEY_FMT, dst);
//	RedisModuleKey *k = RedisModule_OpenKey(ctx, keyName, REDISMODULE_WRITE);
//    RedisModule_ModuleTypeSetValue(ctx, InvertedIndexType, idx);

	  // printf("open key %s: %p\n", RedisModule_StringPtrLen(keyName, NULL), k);
	  // we do not allow empty indexes when loading an existing index
//	  if (k == NULL || RedisModule_ModuleTypeGetType(k) != IndexSpecType) {
//	    return NULL;
//	  }
//	  IndexSpec *sp = RedisModule_ModuleTypeGetValue(k);


//    RedisModule_SendClusterMessage(ctx,NULL,MSGTYPE_PING,(unsigned char*)"Hey",3);

    RedisModule_SendClusterMessage(ctx,NULL,MSGTYPE_EVAL,(unsigned char*)snum, scriptLength + 2);

    // TODO add call EVAL on SELF

//    return RedisModule_ReplyWithSimpleString(ctx, "OK");
    return REDISMODULE_OK;
}

/* Callback for message MSGTYPE_PING */
void EvalReceiver(RedisModuleCtx *ctx, const char *sender_id, uint8_t type, const unsigned char *payload, uint32_t len) {

    RedisModule_Log(ctx,"notice","Eval (type %d) RECEIVED from %.*s: '%.*s'",
        type,REDISMODULE_NODE_ID_LEN,sender_id,(int)len, payload);

    char script[len-1];
    sprintf(script, "%.*s", len-2, payload); // ignore id prefix

    // TODO temp context, remove once https://github.com/antirez/redis/issues/5354 will be fixed
    RedisModuleCtx *tempCtx = RedisModule_GetThreadSafeContext(NULL);

    RedisModuleCallReply *reply = RedisModule_Call(tempCtx,"EVAL", "cc", script, "0");
    RedisModuleString *replyString = RedisModule_CreateStringFromCallReply(reply);

    RedisModule_StringAppendBuffer(tempCtx, replyString, payload+len-2, 2); // copy pingId

    size_t replyLength;
    const char* replyS = RedisModule_StringPtrLen(replyString, &replyLength);


   // RedisModule_Log(ctx,"notice","replyS %s", replyS);


    RedisModule_SendClusterMessage(ctx,sender_id,MSGTYPE_EVAL_RES, replyS, replyLength);

    RedisModule_FreeThreadSafeContext(tempCtx);
}

/* Callback for message MSGTYPE_PONG. */
void EvalResponseReceiver(RedisModuleCtx *ctx, const char *sender_id, uint8_t type, const unsigned char *payload, uint32_t len) {

	RedisModule_Log(ctx,"notice","Eval Response (type %d) RECEIVED from %.*s: '%.*s'",
        type,REDISMODULE_NODE_ID_LEN,sender_id,(int)len, payload);

	// The last 2 chars are call ID
	int id = atoi(&payload[len-2]);
	++clients[id].count;

    RedisModuleCtx *tempCtx = RedisModule_GetThreadSafeContext(NULL); // TODO add automatic memory


	RedisModuleString *keyString = RedisModule_CreateString(tempCtx, clients[id].key, KEY_SIZE);
	RedisModuleString *senderIdString = RedisModule_CreateString(tempCtx, sender_id, REDISMODULE_NODE_ID_LEN);
	RedisModuleKey *key = RedisModule_OpenKey(tempCtx, keyString, REDISMODULE_WRITE);
	RedisModule_HashSet(key, REDISMODULE_HASH_NX, senderIdString, RedisModule_CreateString(tempCtx, payload, len-2), NULL);


    RedisModule_FreeThreadSafeContext(tempCtx);


	if( clients[id].count >= clients[id].numnodes) {
		RedisModuleBlockedClient *bc = clients[id].client;

		int *count = RedisModule_Alloc(sizeof(int));
		*count = clients[id].count;
		RedisModule_UnblockClient(bc, count);
		clients[id].client = NULL;
	}
}

int CreateAvalCommands(RedisModuleCtx *ctx){

    if (RedisModule_CreateCommand(ctx,"rmr.eval", EvalCommand_RedisCommand,"readonly",0,0,0) == REDISMODULE_ERR)
        return REDISMODULE_ERR;

    RedisModule_RegisterClusterMessageReceiver(ctx,MSGTYPE_EVAL,EvalReceiver);
    RedisModule_RegisterClusterMessageReceiver(ctx,MSGTYPE_EVAL_RES,EvalResponseReceiver);

    return REDISMODULE_OK;
}
