#pragma once
#include "rpc.h"
#include <pb.h>
#include <pb_decode.h>
#include <pb_encode.h>
#include <rpc.pb.h>
#include <containers/pipe.h>

/* Forward declarations */
typedef struct FuriString FuriString;
#ifdef __cplusplus
extern "C" {
#endif

typedef void (*RpcMessageHandler)(const Flipper_One_Rpc_RpcMessage* message, void* context);

typedef struct {
    RpcMessageHandler message_handler;
    void* context;
} RpcHandler;

void rpc_send(RpcSession* session, Flipper_One_Rpc_RpcMessage* message);

void rpc_send_and_release(RpcSession* session, Flipper_One_Rpc_RpcMessage* message);

void rpc_add_handler(RpcSession* session, pb_size_t message_tag, RpcHandler* handler);

void rpc_cli_start(PipeSide* pipe, FuriString* args, void* context);

#ifdef __cplusplus
}
#endif
